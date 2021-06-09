#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>

#include <portaudio.h>
#include <pa_utils.h>

#define SAMPLE_RATE 48000  // samples per sec
#define DURATION    4      // secs
#define MAX_DATA    (DURATION * SAMPLE_RATE)

#define DEFAULT_FREQ_START  300
#define MIN_FREQ            100
#define MAX_FREQ            10000

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

static int    freq_start = DEFAULT_FREQ_START;
static int    freq_end;

static bool   done;

static float  data[MAX_DATA];
static int    data_idx;

//
// prototypes
//

static void init_data(void);
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
    PaStreamParameters  output_params;
    PaDeviceIndex       default_output_device_idx;

    // determine freq_start and freq_end
    if (argc >= 2 && sscanf(argv[1], "%d", &freq_start) != 1) {
        printf("ERROR: not a number '%s'\n", argv[1]);
        return 1;
    }
    freq_end = 2 * freq_start;
    if (argc >= 3 && sscanf(argv[2], "%d", &freq_end) != 1) {
        printf("ERROR: not a number '%s'\n", argv[2]);
        return 1;
    }
        
    // validate frequency range
    if (freq_start < MIN_FREQ || freq_start > MAX_FREQ ||
        freq_end < MIN_FREQ || freq_end > MAX_FREQ ||
        freq_end < freq_start)
    {
        printf("ERROR: invalid frequency range: %d - %d\n", freq_start, freq_end);
        return 1;
    }

    // initialize sound data buffer 
    init_data();

    // initalize portaudio
    rc = Pa_Initialize();
    PA_ERROR_CHECK(rc, "Pa_Initialize");

    // get the default output device
    default_output_device_idx = Pa_GetDefaultOutputDevice();
    if (output_params.device == paNoDevice) {
        printf("ERROR: No default output device.\n");
        exit(1);
    }

    // print info
    printf("\nRange %d - %d Hz,  Duration %d secs,  Sample_Rate %d /sec\n\n",
           freq_start, freq_end, DURATION, SAMPLE_RATE);
    pa_print_device_info(default_output_device_idx);

    // init output_params and open the audio output stream
    output_params.device = default_output_device_idx;
    output_params.channelCount              = 1;
    output_params.sampleFormat              = paFloat32;
    output_params.suggestedLatency          = Pa_GetDeviceInfo(output_params.device)->defaultLowOutputLatency;
    output_params.hostApiSpecificStreamInfo = NULL;

    rc = Pa_OpenStream(&stream,
                       NULL,   // input_params
                       &output_params,
                       SAMPLE_RATE,
                       paFramesPerBufferUnspecified,
                       0,       // stream flags
                       stream_cb,
                       NULL);   // user_data
    PA_ERROR_CHECK(rc, "Pa_OpenStream");

    // register callback for when the the audio output compltes
    rc = Pa_SetStreamFinishedCallback(stream, stream_finished_cb);
    PA_ERROR_CHECK(rc, "Pa_SetStreamFinishedCallback");

    // start the audio outuput
    rc = Pa_StartStream(stream);
    PA_ERROR_CHECK(rc, "Pa_StartStream");

    // wait for audio output to complete
    while (!done) {
        Pa_Sleep(10);  // 10 ms
    }

    // clean up and exit
    Pa_StopStream(stream);
    Pa_CloseStream( stream );
    Pa_Terminate();
    return 0;
}

static void init_data(void)
{
    double freq = freq_start;
    int max_data = 0;

    while (true) {
        data[max_data] = sin((2*M_PI) * freq * ((double)max_data/SAMPLE_RATE));
        max_data++;

        freq += (double)(freq_end - freq_start) / (DURATION * SAMPLE_RATE);

        if (max_data == MAX_DATA) {
            if (nearbyint(freq) != freq_end) {
                printf("ERROR: BUG freq %0.3f not equal freq_end %d\n", freq, freq_end);
                exit(1);
            }
            break;
        }
    }
}

// -----------------  CALLBACKS  --------------------------------------------------

static int stream_cb(const void *input,
                     void *output,
                     unsigned long frame_count,
                     const PaStreamCallbackTimeInfo *timeinfo,
                     PaStreamCallbackFlags status_flags,
                     void *user_data)
{
    float *out = output;
    int data_remaining = MAX_DATA - data_idx;

    if (data_remaining < frame_count) {
        return paComplete;
    }

    memcpy(out, data+data_idx, frame_count*sizeof(float));
    data_idx += frame_count;
    
    return paContinue;
}

static void stream_finished_cb(void *userData)
{
    done = true;
}
