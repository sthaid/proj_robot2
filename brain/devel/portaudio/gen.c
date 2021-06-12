#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>

#include <portaudio.h>
#include <pa_utils.h>

#define SAMPLE_RATE         48000  // samples per sec
#define DURATION            5      // secs
#define DEFAULT_FREQ_START  300
#define MIN_FREQ            100
#define MAX_FREQ            10000

#define MAX_DATA            (DURATION * SAMPLE_RATE)

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
    PaDeviceIndex       devidx;
    char               *output_device = DEFAULT_OUTPUT_DEVICE;

    // usage: gen [-d outdev] [freq_start] [freq_end]

    // parse options
    while (true) {
	int ch = getopt(argc, argv, "d:h");
	if (ch == -1) {
	    break;
	}
	switch (ch) {
	case 'd':
	    output_device = optarg;
	    break;
	case 'h':
	    printf("usage: gen [-d outdev] [freq_start] [freq_end]\n");
	    return 0;
	    break;
	default:
	    return 1;
	};
    }

    // determine freq_start and freq_end
    if (argc-optind >= 1 && sscanf(argv[optind], "%d", &freq_start) != 1) {
        printf("ERROR: not a number '%s'\n", argv[optind]);
        return 1;
    }
    freq_end = 2 * freq_start;
    if (argc-optind >= 2 && sscanf(argv[optind+1], "%d", &freq_end) != 1) {
        printf("ERROR: not a number '%s'\n", argv[optind+1]);
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

    // get the output device idx
    devidx = pa_find_device(output_device);
    if (devidx == paNoDevice) {
        printf("ERROR: could not find %s\n", output_device);
        exit(1);
    }

    // print info
    printf("\nRange %d - %d Hz,  Duration %d secs,  Sample_Rate %d /sec\n\n",
           freq_start, freq_end, DURATION, SAMPLE_RATE);
    pa_print_device_info(devidx);

    // init output_params and open the audio output stream
    output_params.device            = devidx;
    output_params.channelCount      = 2;
    output_params.sampleFormat      = paFloat32 | paNonInterleaved;
    output_params.suggestedLatency  = Pa_GetDeviceInfo(output_params.device)->defaultLowOutputLatency;
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
    float **out = (void*)output;

    // if more frames are requested than we have remaining data for then return paComplete
    if (frame_count > MAX_DATA - data_idx) {
        return paComplete;
    }

    // left channel - not used
    memset(out[0], 0, frame_count*sizeof(float));

    // right channel
    memcpy(out[1], data+data_idx, frame_count*sizeof(float));

    // increase data_idx by the frame_count
    data_idx += frame_count;
    
    // continue
    return paContinue;
}

static void stream_finished_cb(void *userData)
{
    done = true;
}
