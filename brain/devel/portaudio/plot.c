#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <portaudio.h>

#include <util_sdl.h>
#include <util_misc.h>
#include <pa_utils.h>

//
// defines
//

#define SAMPLE_RATE 48000  // samples per sec
#define DURATION    1      // secs
#define MAX_CHANNEL 4
#define MAX_DATA    (DURATION * SAMPLE_RATE)

//
// variables
//

static int    win_width = 1500;
static int    win_height = 800;
static int    opt_fullscreen = false;

static bool   data_ready;
static float  data[MAX_CHANNEL][MAX_DATA];

//
// prototypes
//

static int pane_hndlr(pane_cx_t *pane_cx, int request, void * init_params, sdl_event_t * event);
static int get_mic_data_init(void);
static void get_mic_data_terminate(void);
static int get_mic_data_start(void);

// -----------------  MAIN  ---------------------------------------------------------

int main(int argc, char **argv)
{
    // init portaudio to get data from the respeaker 4 channel microphone array
    if (get_mic_data_init() < 0) {
        FATAL("get_mic_data_init\n");
    }

    // init sdl
    if (sdl_init(&win_width, &win_height, opt_fullscreen, false, false) < 0) {
        FATAL("sdl_init %dx%d failed\n", win_width, win_height);
    }

    // run the pane manger, this is the runtime loop
    sdl_pane_manager(
        NULL,           // context
        NULL,           // called prior to pane handlers
        NULL,           // called after pane handlers
        10000,          // 0=continuous, -1=never, else us
        1,              // number of pane handler varargs that follow
        pane_hndlr, NULL, 0, 0, win_width, win_height, PANE_BORDER_STYLE_NONE);

    // program terminating
    get_mic_data_terminate();
    INFO("TERMINATING\n");
    return 0;
}

// -----------------  PANE HNDLR  ---------------------------------------------------

static int pane_hndlr(pane_cx_t * pane_cx, int request, void * init_params, sdl_event_t * event)
{
    rect_t *pane = &pane_cx->pane;

    // ----------------------------
    // -------- INITIALIZE --------
    // ----------------------------

    if (request == PANE_HANDLER_REQ_INITIALIZE) {
        return PANE_HANDLER_RET_NO_ACTION;
    }

    // ------------------------
    // -------- RENDER --------
    // ------------------------

    if (request == PANE_HANDLER_REQ_RENDER) {
        sdl_render_printf(pane, 0, 0, 50, SDL_WHITE, SDL_BLUE, "Hello World!");
        return PANE_HANDLER_RET_NO_ACTION;
    }

    // -----------------------
    // -------- EVENT --------
    // -----------------------

    if (request == PANE_HANDLER_REQ_EVENT) {
        switch (event->event_id) {
        case 'g':
            get_mic_data_start();
            break;
        default:
            INFO("got event_id 0x%x\n", event->event_id);
            break;
        }
//xxx get the data       g
//xxx scroll x           left right arrows     change start sample
//xxx change x scale     + -                   change sample_per_pixel
//xxx change y scale     shift + -             change maxy

        return PANE_HANDLER_RET_NO_ACTION;
    }

    // ---------------------------
    // -------- TERMINATE --------
    // ---------------------------

    if (request == PANE_HANDLER_REQ_TERMINATE) {
        return PANE_HANDLER_RET_NO_ACTION;
    }

    // not reached
    FATAL("not reached\n");
    return PANE_HANDLER_RET_NO_ACTION;
}

// -----------------  GET MIC DATA  -------------------------------------------------

static int       data_idx;
static PaStream *stream;

static void get_mic_data_stream_finished_cb(void *userData);
static int get_mic_data_stream_cb(const void *input,
                                  void *output,
                                  unsigned long frame_count,
                                  const PaStreamCallbackTimeInfo *timeinfo,
                                  PaStreamCallbackFlags status_flags,
                                  void *user_data);

static int get_mic_data_init(void)
{
    PaError             rc;
    PaStreamParameters  input_params;
    PaDeviceIndex       default_input_device_idx;
    const PaDeviceInfo *di;

    // initalize portaudio
    rc = Pa_Initialize();
    if (rc < 0) {
        ERROR("%s rc=%d, %s\n", "Pa_Initialize", rc, Pa_GetErrorText(rc));
        return -1;
    }

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
                       get_mic_data_stream_cb,
                       NULL);   // user_data
    if (rc < 0) {
        ERROR("%s rc=%d, %s\n", "Pa_OpenStream", rc, Pa_GetErrorText(rc));
        return -1;
    }

    // register callback for when the the audio output compltes
    rc = Pa_SetStreamFinishedCallback(stream, get_mic_data_stream_finished_cb);
    if (rc < 0) {
        ERROR("%s rc=%d, %s\n", "Pa_SetStreamFinishedCallback", rc, Pa_GetErrorText(rc));
        return -1;
    }

    return 0;
}

static void get_mic_data_terminate(void)
{
    // clean up and terminate portaudio
    Pa_StopStream(stream);
    Pa_CloseStream( stream );
    Pa_Terminate();
}

static int get_mic_data_start(void)
{
    int rc;
    bool okay;
    static bool first_call = true;

    INFO("mic data capture starting\n");

    // if stream is running return an error
    okay = first_call || data_ready;
    first_call = false;
    if (!okay) {
        ERROR("busy, can't start\n");
        return -1;
    }

    // reset variables when starting mic data capture
    data_idx = 0;
    data_ready = false;
    
    // start the audio input
    rc = Pa_StartStream(stream);
    if (rc < 0) {
        FATAL("%s rc=%d, %s\n", "Pa_SetStreamFinishedCallback", rc, Pa_GetErrorText(rc));
    }

    // success
    return 0;
}

static int get_mic_data_stream_cb(const void *input,
                                  void *output,
                                  unsigned long frame_count,
                                  const PaStreamCallbackTimeInfo *timeinfo,
                                  PaStreamCallbackFlags status_flags,
                                  void *user_data)
{
    float **in = (void*)input;
    int chan;

    // sanity check data_idx
    if (data_idx < 0 || data_idx >= MAX_DATA) {
        FATAL("unexpected data_idx=%d\n", data_idx);
    }

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

static void get_mic_data_stream_finished_cb(void *userData)
{
    INFO("mic data capture complete\n");
    data_ready = true;
}
