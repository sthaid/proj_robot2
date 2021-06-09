#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>

#include <portaudio.h>
#include <pa_utils.h>
#include <sf_utils.h>

#define SAMPLE_RATE 48000  // samples per sec
#define DURATION    4      // secs
#define MAX_DATA    (DURATION * SAMPLE_RATE)

#define MAX_CHANNEL 4

#define PA_ERROR_CHECK(rc, routine_name) \
    do { \
        if (rc != paNoError) { \
            printf("ERROR: %s rc=%d, %s\n", routine_name, rc, Pa_GetErrorText(rc)); \
            Pa_Terminate(); \
            exit(1); \
        } \
    } while (0)

//
// variables
//

static bool   done;
static float  data[MAX_CHANNEL][MAX_DATA];

//
// prototypes
//

static int stream_cb(const void *input,
                     void *output,
                     unsigned long frame_count,
                     const PaStreamCallbackTimeInfo *timeinfo,
                     PaStreamCallbackFlags status_flags,
                     void *user_data);
static void stream_finished_cb(void *userData);

// -----------------  MAIN  -------------------------------------------------------

int main(int argc, char **argv)
{
    PaError             rc;
    PaStream           *stream;
    PaStreamParameters  input_params;
    PaDeviceIndex       default_input_device_idx;
    const PaDeviceInfo *di;

    // initalize portaudio
    rc = Pa_Initialize();
    PA_ERROR_CHECK(rc, "Pa_Initialize");

    // get the default input device, and print info
    default_input_device_idx = Pa_GetDefaultInputDevice();
    if (input_params.device == paNoDevice) {
        printf("ERROR: No default output device.\n");
        exit(1);
    }
    printf("\n");
    pa_print_device_info(default_input_device_idx);

    // confirm input device is seeed-4mic-voicecard
    di = Pa_GetDeviceInfo(default_input_device_idx);
    if (strncmp(di->name, "seeed-4mic-voicecard", 20) != 0) {
        printf("ERROR: name=%s, must be 'seeed-4mic-voicecard'\n", di->name);
        exit(1);
    }
    if (di->maxInputChannels != MAX_CHANNEL) {
        printf("ERROR: maxInputChannels=%d, must be %d\n", di->maxInputChannels, MAX_CHANNEL);
        exit(1);
    }

    // init input_params and open the audio output stream
    input_params.device           = default_input_device_idx;
    input_params.channelCount     = MAX_CHANNEL;
    input_params.sampleFormat     = paFloat32 | paNonInterleaved;
    input_params.suggestedLatency = Pa_GetDeviceInfo(input_params.device)->defaultLowInputLatency;
    input_params.hostApiSpecificStreamInfo = NULL;

    rc = Pa_OpenStream(&stream,
                       &input_params,
                       NULL,   // output_params
                       SAMPLE_RATE,
                       paFramesPerBufferUnspecified,
                       0,       // stream flags
                       stream_cb,
                       NULL);   // user_data
    PA_ERROR_CHECK(rc, "Pa_OpenStream");

    // register callback for when the the audio output compltes
    rc = Pa_SetStreamFinishedCallback(stream, stream_finished_cb);
    PA_ERROR_CHECK(rc, "Pa_SetStreamFinishedCallback");

    // start the audio input
    rc = Pa_StartStream(stream);
    PA_ERROR_CHECK(rc, "Pa_StartStream");

    // wait for audio output to complete
    while (!done) {
        Pa_Sleep(10);  // 10 ms
    }

    // clean up and terminate portaudio
    Pa_StopStream(stream);
    Pa_CloseStream( stream );
    Pa_Terminate();

    // print average data for each channel
    printf("\n");
    for (int chan = 0; chan < MAX_CHANNEL; chan++) {
        float sum = 0;
        for (int i = 0; i < MAX_DATA; i++) {
            sum += fabsf(data[chan][i]);
        }
        printf("avg data for chan %d = %0.3f\n", chan, sum/MAX_DATA);
    }

    // write wav file for each channel
    for (int chan = 0; chan < MAX_CHANNEL; chan++) {
        char filename[100];
        sprintf(filename, "record_%d.wav", chan);
        sf_create_wav(data[chan], MAX_DATA, SAMPLE_RATE, filename);
    }

    // done
    return 0;
}

// -----------------  CALLBACKS  --------------------------------------------------

static int stream_cb(const void *input,
                     void *output,
                     unsigned long frame_count,
                     const PaStreamCallbackTimeInfo *timeinfo,
                     PaStreamCallbackFlags status_flags,
                     void *user_data)
{
    float **in = (void*)input;
    int chan;

    static int data_idx;

    // reduce frame_count if there is not enough space remaining in data
    if (frame_count > MAX_DATA - data_idx) {
        frame_count = MAX_DATA - data_idx;
    }

    // for each channel make a copy of the input data
    for (chan = 0; chan < MAX_CHANNEL; chan++) {
        memcpy(&data[chan][data_idx], in[chan], frame_count*sizeof(float));
    }

    // update data_idx
    data_idx += frame_count;

    // return either paComplete or paContinue
    return data_idx == MAX_DATA ? paComplete : paContinue;
}

static void stream_finished_cb(void *userData)
{
    done = true;
}
