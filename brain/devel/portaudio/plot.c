#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <portaudio.h>

#include <util_sdl.h>
#include <util_misc.h>
#include <pa_utils.h>

//
// defines
//

#define INPUT_DEVICE "seeed-4mic-voicecard"
#define MAX_CHANNEL  4
#define SAMPLE_RATE  48000  // samples per sec
#define DURATION     1      // secs

#define MAX_DATA     (DURATION * SAMPLE_RATE)

#define GET_MIC_DATA_FILENAME "get_mic_data.dat"

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
static int get_mic_data_read_file(void);
static int get_mic_data_write_file(void);

// -----------------  MAIN  ---------------------------------------------------------

int main(int argc, char **argv)
{
    // init portaudio to get data from the respeaker 4 channel microphone array
    // xxx comment on WARN
    if (get_mic_data_init() < 0) {
        WARN("get_mic_data_init failed\n");
    }

    // xxx
    get_mic_data_read_file();

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

static int x_start = 10000;
static int x_cursor;

static void plot(rect_t *pane, int chan);

static int pane_hndlr(pane_cx_t * pane_cx, int request, void * init_params, sdl_event_t * event)
{
    rect_t *pane = &pane_cx->pane;

    // ----------------------------
    // -------- INITIALIZE --------
    // ----------------------------

    if (request == PANE_HANDLER_REQ_INITIALIZE) {
	x_cursor = pane->w / 2;
        return PANE_HANDLER_RET_NO_ACTION;
    }

    // ------------------------
    // -------- RENDER --------
    // ------------------------

    if (request == PANE_HANDLER_REQ_RENDER) {
        if (data_ready) {
            for (int chan = 0; chan < MAX_CHANNEL; chan++) {
                plot(pane, chan); 
            }
        } else {
            // xxx ctr
            sdl_render_printf(pane, 0, 0, 50, SDL_WHITE, SDL_BLUE, 
                              "No Data");
        }

	sdl_render_line(pane, x_cursor, 0, x_cursor, pane->h-1, SDL_WHITE);

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
	case SDL_EVENT_KEY_LEFT_ARROW:
	    x_start++;
	    break;
	case SDL_EVENT_KEY_RIGHT_ARROW:
	    x_start--;
	    break;
	case '<':
	    x_cursor++;
	    break;
	case '>':
	    x_cursor--;
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

static void plot(rect_t *pane, int chan)
{
    int max_p = 0;
    int x, y_base;
    point_t p[10000];
    int x_data_idx;

    y_base = (pane->h / 4) * (0.5 + chan);
    //INFO("chan=%d y_base=%d\n", chan, y_base);

    x = 0;
    for (x_data_idx = x_start; true; x_data_idx++) {
        p[max_p].x = x;
        p[max_p].y = y_base + data[chan][x_data_idx] * (pane->h/8);
        max_p++;
	x += 10;   // XXX
	if (x >= pane->w) {
	    break;
	}
    }

    sdl_render_line(pane, 0, y_base, pane->w-1, y_base, SDL_WHITE);
    sdl_render_lines(pane, p, max_p, SDL_WHITE);
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
    PaDeviceIndex       devidx;

    // initalize portaudio
    rc = Pa_Initialize();
    if (rc < 0) {
        ERROR("%s rc=%d, %s\n", "Pa_Initialize", rc, Pa_GetErrorText(rc));
        return -1;
    }

    // get the input device idx
    // xxx confirm number of channels
    devidx = pa_find_device(INPUT_DEVICE);
    if (devidx == paNoDevice) {
        ERROR("could not find %s\n", INPUT_DEVICE);
        return -1;
    }

    // init input_params and open the audio output stream
    input_params.device           = devidx;
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
    if (stream) {
	Pa_StopStream(stream);
	Pa_CloseStream(stream);
    }
    Pa_Terminate();
}

static int get_mic_data_start(void)
{
    int rc;
    bool okay;
    static bool first_call = true;

    INFO("mic data capture starting\n");

    if (stream == NULL) {
	ERROR("stream not open\n");
	return -1;
    }

    // if stream is running return an error
    okay = first_call || data_ready;
    first_call = false;
    if (!okay) {
        ERROR("busy, can't start\n");
        return -1;
    }

    // reset for next mic data capture
    Pa_StopStream(stream);
    data_idx = 0;
    data_ready = false;
    
    // start the audio input
    rc = Pa_StartStream(stream);
    if (rc < 0) {
        FATAL("%s rc=%d, %s\n", "Pa_StartStream", rc, Pa_GetErrorText(rc));
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

    // xxx should the read and write be both outside this section
    get_mic_data_write_file();

    data_ready = true;
}

static int get_mic_data_read_file(void)
{
    int fd, len;

    INFO("reading %s\n", GET_MIC_DATA_FILENAME);

    fd = open(GET_MIC_DATA_FILENAME, O_RDONLY);
    if (fd < 0) {
        WARN("open %s, %s\n", GET_MIC_DATA_FILENAME, strerror(errno));
        return -1;
    }

    len = read(fd, data, sizeof(data));
    if (len != sizeof(data)) {
        WARN("read %s len=%d, %s\n", GET_MIC_DATA_FILENAME, len, strerror(errno));
        return -1;
    }

    close(fd);
    data_ready = true;
    return 0;
}

static int get_mic_data_write_file(void)
{
    int fd, len;

// xxx data must be ready
    INFO("writing %s\n", GET_MIC_DATA_FILENAME);

    fd = open(GET_MIC_DATA_FILENAME, O_CREAT|O_TRUNC|O_WRONLY, 0666);
    if (fd < 0) {
        ERROR("open %s, %s\n", GET_MIC_DATA_FILENAME, strerror(errno));
        return -1;
    }

    len = write(fd, data, sizeof(data));
    if (len != sizeof(data)) {
        WARN("write %s len=%d, %s\n", GET_MIC_DATA_FILENAME, len, strerror(errno));
        return -1;
    }

    close(fd);
    return 0;
}
