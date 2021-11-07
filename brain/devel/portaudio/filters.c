// FFTW NOTES:
//
// References:
// - http://www.fftw.org/
// - http://www.fftw.org/fftw3.pdf
//
// Install on Ubuntu
// - sudo apt install libfftw3-dev
//
// A couple of general notes from the pdf
// - Size that is the products of small factors transform more efficiently.
// - You must create the plan before initializing the input, because FFTW_MEASURE 
//   overwrites the in/out arrays.
// - The DFT results are stored in-order in the array out, with the zero-frequency (DC) 
//   component in out[0].

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <complex.h>
#include <math.h>
#include <pthread.h>
#include <fftw3.h>

#include <audio_filters.h>
#include <pa_utils.h>
#include <sf_utils.h>

#include <util_sdl.h>
#include <util_misc.h>

//
// defines
//

#define SAMPLE_RATE 48000  // samples per sec
#define DURATION    5      // secs
#define N           (DURATION * SAMPLE_RATE)

#define TIME(code) \
    ( { unsigned long start=microsec_timer(); code; (microsec_timer()-start)/1000000.; } )

//
// variables
//

static int         win_width = 1600;
static int         win_height = 800;
static int         opt_fullscreen = false;

static complex    *in_data;
static complex    *in;
static complex    *out;
static fftw_plan   plan;

static int         lpf_k1;
static double      lpf_k2;
static int         hpf_k1;
static double      hpf_k2;

static char       *file_name;
static float      *file_data;
static int         file_max_chan;
static int         file_max_data;
static int         file_sample_rate;

static char       *audio_out_dev = DEFAULT_OUTPUT_DEVICE;
static int         audio_out_filter;
static double      audio_out_volume[4];
static bool        audio_out_auto_volume;

static int         plot_scale[4];

//
// prototypes
//

static void reset_params(void);
static void init_in_data(int type);
static void *audio_out_thread(void *cx);
static int audio_out_get_frame(void *data, void *cx);
static int pane_hndlr(pane_cx_t * pane_cx, int request, void * init_params, sdl_event_t * event);
static int plot(rect_t *pane, int idx, complex *data, int n);
static void apply_low_pass_filter(complex *data, int n);
static void apply_high_pass_filter(complex *data, int n);
static void apply_band_pass_filter(complex *data, int n);
static void clip_int(int *v, int low, int high);
static void clip_double(double *v, double low, double high);
static int clip_meter(complex *data, int n, double vol, double *max);

// -----------------  MAIN  --------------------------------------

int main(int argc, char **argv)
{
    pthread_t tid;

    #define USAGE \
    "usage: filters [-f file_name.wav] [-d out_dev] -h"  // XXX better usage comment

    // parse options
    while (true) {
        int ch = getopt(argc, argv, "f:d:h");
        if (ch == -1) {
            break;
        }
        switch (ch) {
        case 'f':
            file_name = optarg;
            break;
        case 'd':
            audio_out_dev = optarg;
            break;
        case 'h':
            printf("%s\n", USAGE);
            return 0;
        default:
            return 1;
        }
    }

    // print args
    printf("INFO: audio_out_dev=%s file_name=%s\n", audio_out_dev, file_name);

    // init params
    reset_params();

    // if file_name provided then read the wav file;
    // this file's data will be one of the 'in' data sources
    if (file_name) {
        if (strstr(file_name, ".wav") == NULL) {
            printf("FATAL: file_name must have '.wav' extension\n");
            return 1;
        }
        if (sf_read_wav_file(file_name, &file_data, &file_max_chan, &file_max_data, &file_sample_rate) < 0) {
            printf("FATAL: sf_read_wav_file %s failed\n", optarg);
            return 1;
        }
        if (file_sample_rate != SAMPLE_RATE) {
            printf("FATAL: file smaple_rate=%d, must be %d\n", file_sample_rate, SAMPLE_RATE);
            return 1;
        }
        if (file_max_data/file_max_chan < N) {
            printf("FATAL: file frames=%d (%d / %d) is < %d\n",
                   file_max_data/file_max_chan, file_max_data, file_max_chan, N);
            return 1;
        }
    }

    // init portaudio
    if (pa_init() < 0) {
        printf("FATAL: pa_init\n");
        return 1;
    }

    // allocate in and out arrays in create the plan
    in  = (complex*)fftw_malloc(sizeof(complex) * N);
    out = (complex*)fftw_malloc(sizeof(complex) * N);
    plan = fftw_plan_dft_1d(N, in, out, FFTW_FORWARD, FFTW_ESTIMATE);

    // init data, use file_data if avail, else white noise
    in_data  = (complex*)fftw_malloc(sizeof(complex) * N);
    init_in_data(file_data ? '3': '2');

    // create audio_out_thread
    pthread_create(&tid, NULL, audio_out_thread, NULL);

    // init sdl
    if (sdl_init(&win_width, &win_height, opt_fullscreen, false, false) < 0) {
        printf("FATAL: sdl_init %dx%d failed\n", win_width, win_height);
        return 1;
    }

    // run the pane manger, this is the runtime loop
    sdl_pane_manager(
        NULL,           // context
        NULL,           // called prior to pane handlers
        NULL,           // called after pane handlers
        10000,          // 0=continuous, -1=never, else us
        1,              // number of pane handler varargs that follow
        pane_hndlr, NULL, 0, 0, win_width, win_height, PANE_BORDER_STYLE_NONE);

    // clean up
    fftw_destroy_plan(plan);
    fftw_free(in); fftw_free(out);

    // terminate
    return 0;
}

static void reset_params(void)
{
    int i;

    // tuned for midrange (bandpass) filter 500 to 2000 hz
    lpf_k1 = 7;
    lpf_k2 = 0.85;
    hpf_k1 = 5;
    hpf_k2 = 0.85;

    audio_out_auto_volume = true;
    for (i = 0; i < 4; i++) {
        audio_out_volume[i] = 0;
    }

    for (i = 0; i < 4; i++) {
        plot_scale[i] = 1;
    }
}

// -----------------  INIT IN DATA ARRAY  ------------------------

// arg: 'type'
// - F1, F2: tones 100 ... 5000 Hz
// - '1':    white noise
// - '2':    mix of sine waves
// - '3':    file-data
static void init_in_data(int type)
{
    int i, j, freq;
    static int static_freq = 100;

    // fill the in_data array, based on 'type' arg
    switch (type) {
    case SDL_EVENT_KEY_F(1) ... SDL_EVENT_KEY_F(2): 
        if (type == SDL_EVENT_KEY_F(1)) {
            static_freq -= 100;
        } else {
            static_freq += 100;
        }
        clip_int(&static_freq, 100, 5000);
        for (i = 0; i < N; i++) {
            in_data[i] = sin((2*M_PI) * static_freq * ((double)i/SAMPLE_RATE));
        }
        break;
    case '1':  // white nosie
        for (i = 0; i < N; i++) {
            in_data[i] = ((double)random() / RAND_MAX) - 0.5;
        }
        break;
    case '2':  // sum of sine waves
        memset(in_data, 0, N*sizeof(complex));
        for (freq = 25; freq <= 5000; freq += 25) {
            for (i = 0; i < N; i++) {
                in_data[i] += sin((2*M_PI) * freq * ((double)i/SAMPLE_RATE));
            }
        }
        break;
    case '3':  // file data
        if (file_max_data == 0) {
            printf("WARNING: no file data\n");
            break;
        }
        for (i = 0, j = 0; i < N; i++) {
            in_data[i] = file_data[j];
            j += file_max_chan;
        }
        break;
    default:
        printf("FATAL: init_in_data, invalid type=%d\n", type);
        exit(1);
        break;
    }

    // scale in_data values to range -1 to 1
    double max=0, v;
    for (i = 0; i < N; i++) {
        v = fabs(creal(in_data[i]));
        if (v > max) max = v;
    }
    for (i = 0; i < N; i++) {
        in_data[i] *= (1 / max);
    }
}

// -----------------  PLAY FILTERED AUDIO  -----------------------

static void *audio_out_thread(void *cx)
{
    if (pa_play2(audio_out_dev, 1, SAMPLE_RATE, PA_FLOAT32, audio_out_get_frame, NULL) < 0) {
        printf("FATAL: pa_play2 failed\n");
        exit(1); 
    }
    return NULL;
}

static int audio_out_get_frame(void *out_data_arg, void *cx_arg)
{
    double vd, vo;
    float *out_data = out_data_arg;

    static double cx[200];
    static int idx;

    // get next in_data value
    vd = creal(in_data[idx]);
    idx = (idx + 1) % N;

    // filter the value
    switch (audio_out_filter) {
    case 0:
        vo = vd;
        break;
    case 1:
        vo = low_pass_filter_ex(vd, cx, lpf_k1, lpf_k2);
        break;
    case 2:
        vo = high_pass_filter_ex(vd, cx, hpf_k1, hpf_k2);
        break;
    case 3:
        vo = band_pass_filter_ex(vd, cx, lpf_k1, lpf_k2, hpf_k1, hpf_k2);
        break;
    default:
        printf("FATAL: audio_out_filter=%d\n", audio_out_filter);
        exit(1);
        break;
    }

    // scale the filtered value by volume
    vo *= audio_out_volume[audio_out_filter];

    // return the filtered value
    out_data[0] = vo;

    // continue 
    return 0;
}

// -----------------  PANE HNDLR----------------------------------

static int pane_hndlr(pane_cx_t * pane_cx, int request, void * init_params, sdl_event_t * event)
{
    rect_t *pane = &pane_cx->pane;

    #define SDL_EVENT_LPF_K1           (SDL_EVENT_USER_DEFINED + 0)
    #define SDL_EVENT_LPF_K2           (SDL_EVENT_USER_DEFINED + 1)
    #define SDL_EVENT_HPF_K1           (SDL_EVENT_USER_DEFINED + 2)
    #define SDL_EVENT_HPF_K2           (SDL_EVENT_USER_DEFINED + 3)

    #define SDL_EVENT_AUDIO_OUT_UNF    (SDL_EVENT_USER_DEFINED + 10)
    #define SDL_EVENT_AUDIO_OUT_LPF    (SDL_EVENT_USER_DEFINED + 11)
    #define SDL_EVENT_AUDIO_OUT_HPF    (SDL_EVENT_USER_DEFINED + 12)
    #define SDL_EVENT_AUDIO_OUT_BPF    (SDL_EVENT_USER_DEFINED + 13)

    #define SDL_EVENT_UNF_VOLUME       (SDL_EVENT_USER_DEFINED + 20)
    #define SDL_EVENT_LPF_VOLUME       (SDL_EVENT_USER_DEFINED + 21)
    #define SDL_EVENT_HPF_VOLUME       (SDL_EVENT_USER_DEFINED + 22)
    #define SDL_EVENT_BPF_VOLUME       (SDL_EVENT_USER_DEFINED + 23)

    #define SDL_EVENT_PLOT_SCALE_UNF   (SDL_EVENT_USER_DEFINED + 30)
    #define SDL_EVENT_PLOT_SCALE_LPF   (SDL_EVENT_USER_DEFINED + 31)
    #define SDL_EVENT_PLOT_SCALE_HPF   (SDL_EVENT_USER_DEFINED + 32)
    #define SDL_EVENT_PLOT_SCALE_BPF   (SDL_EVENT_USER_DEFINED + 33)

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
        char str[100];
        int y_origin;
        double t1;

        #define FSZ    30
        #define X_TEXT (pane->w-195)

        #define DISPLAY_COMMON(title, idx, ev_audio_vol, ev_audio_out_slct, ev_plot_scale) \
            do { \
                double data_max_val; \
                int cm; \
                /* display title line */ \
                sdl_render_printf(pane, X_TEXT, y_origin-ROW2Y(6,FSZ), FSZ, SDL_WHITE, SDL_BLACK,  \
                                  title " %d", (int)nearbyint(t1*1000)); \
                /* display volume */ \
                sprintf(str, "%0.2f", audio_out_volume[idx]); \
                if (audio_out_auto_volume) { \
                    sdl_render_printf(pane, X_TEXT, y_origin-ROW2Y(5,FSZ), FSZ, SDL_WHITE, SDL_BLACK, "%s", str); \
                } else { \
                    sdl_render_text_and_register_event( \
                        pane,  \
                        X_TEXT, y_origin-ROW2Y(5,FSZ), \
                        FSZ, str, SDL_LIGHT_BLUE, SDL_BLACK,  \
                        ev_audio_vol, SDL_EVENT_TYPE_MOUSE_WHEEL, pane_cx); \
                } \
                /* display clip meter */ \
                cm = clip_meter(in, N, audio_out_volume[idx], &data_max_val); \
                if (audio_out_auto_volume || audio_out_volume[idx] == 0) { \
                    audio_out_volume[idx] = 0.99 / data_max_val; \
                } \
                sdl_render_fill_rect(pane, \
                                      &(rect_t){ pane->w-90, y_origin-ROW2Y(5,FSZ), cm*15, FSZ }, \
                                     SDL_RED); \
                /* display plot scale */ \
                if (plot_scale[idx] != 1) { \
                    sdl_render_printf(pane, 0, y_origin-ROW2Y(1,FSZ), FSZ, SDL_WHITE, SDL_BLACK, \
                                      "SCALE=%d", plot_scale[idx]); \
                } \
                /* register for audio out select mouse click event */ \
                sdl_register_event(pane, \
                                   &(rect_t){ 0, y_origin-180, pane->w-200, 180 }, \
                                   ev_audio_out_slct, SDL_EVENT_TYPE_MOUSE_CLICK, pane_cx); \
                /* register for plot scale mouse wheel event */ \
                sdl_register_event(pane, \
                                   &(rect_t){ 0, y_origin-180, pane->w-200, 180 }, \
                                   ev_plot_scale, SDL_EVENT_TYPE_MOUSE_WHEEL, pane_cx); \
            } while (0)

        // ----------------------
        // plot fft of unfiltered 'in' data
        // ----------------------
        memcpy(in, in_data, N*sizeof(complex));
        t1 = TIME(fftw_execute(plan));
        y_origin = plot(pane, 0, out, N);

        DISPLAY_COMMON("UNF", 0, SDL_EVENT_UNF_VOLUME, SDL_EVENT_AUDIO_OUT_UNF, SDL_EVENT_PLOT_SCALE_UNF);

        // -------------------------------------
        // plot fft of low pass filtered 'in' data
        // -------------------------------------
        memcpy(in, in_data, N*sizeof(complex));
        t1 = TIME(apply_low_pass_filter(in, N));
        fftw_execute(plan);
        y_origin = plot(pane, 1, out, N);

        DISPLAY_COMMON("LPF", 1, SDL_EVENT_LPF_VOLUME, SDL_EVENT_AUDIO_OUT_LPF, SDL_EVENT_PLOT_SCALE_LPF);

        sprintf(str, "%4d", lpf_k1);
        sdl_render_text_and_register_event(
            pane, 
            X_TEXT, y_origin-ROW2Y(2,FSZ),
            FSZ, str, SDL_LIGHT_BLUE, SDL_BLACK, 
            SDL_EVENT_LPF_K1, SDL_EVENT_TYPE_MOUSE_WHEEL, pane_cx);

        sprintf(str, "%4.2f", lpf_k2);
        sdl_render_text_and_register_event(
            pane, 
            X_TEXT, y_origin-ROW2Y(1,FSZ),
            FSZ, str, SDL_LIGHT_BLUE, SDL_BLACK, 
            SDL_EVENT_LPF_K2, SDL_EVENT_TYPE_MOUSE_WHEEL, pane_cx);

        // -------------------------------------
        // plot fft of high pass filtered 'in' data
        // -------------------------------------
        memcpy(in, in_data, N*sizeof(complex));
        t1 = TIME(apply_high_pass_filter(in, N));
        fftw_execute(plan);
        y_origin = plot(pane, 2, out, N);

        DISPLAY_COMMON("HPF", 2, SDL_EVENT_HPF_VOLUME, SDL_EVENT_AUDIO_OUT_HPF, SDL_EVENT_PLOT_SCALE_HPF);

        sprintf(str, "%4d", hpf_k1);
        sdl_render_text_and_register_event(
            pane, 
            X_TEXT, y_origin-ROW2Y(2,FSZ),
            FSZ, str, SDL_LIGHT_BLUE, SDL_BLACK, 
            SDL_EVENT_HPF_K1, SDL_EVENT_TYPE_MOUSE_WHEEL, pane_cx);

        sprintf(str, "%4.2f", hpf_k2);
        sdl_render_text_and_register_event(
            pane, 
            X_TEXT, y_origin-ROW2Y(1,FSZ),
            FSZ, str, SDL_LIGHT_BLUE, SDL_BLACK, 
            SDL_EVENT_HPF_K2, SDL_EVENT_TYPE_MOUSE_WHEEL, pane_cx);

        // -------------------------------------
        // plot fft of band pass filtered 'in' data
        // -------------------------------------
        memcpy(in, in_data, N*sizeof(complex));
        t1 = TIME(apply_band_pass_filter(in, N));
        fftw_execute(plan);
        y_origin = plot(pane, 3, out, N);

        DISPLAY_COMMON("BPF", 3, SDL_EVENT_BPF_VOLUME, SDL_EVENT_AUDIO_OUT_BPF, SDL_EVENT_PLOT_SCALE_BPF);

        sprintf(str, "%4d", lpf_k1);
        sdl_render_text_and_register_event(
            pane, 
            X_TEXT, y_origin-ROW2Y(4,FSZ),
            FSZ, str, SDL_LIGHT_BLUE, SDL_BLACK,
            SDL_EVENT_LPF_K1, SDL_EVENT_TYPE_MOUSE_WHEEL, pane_cx);

        sprintf(str, "%4.2f", lpf_k2);
        sdl_render_text_and_register_event(
            pane, 
            X_TEXT, y_origin-ROW2Y(3,FSZ),
            FSZ, str, SDL_LIGHT_BLUE, SDL_BLACK,
            SDL_EVENT_LPF_K2, SDL_EVENT_TYPE_MOUSE_WHEEL, pane_cx);

        sprintf(str, "%4d", hpf_k1);
        sdl_render_text_and_register_event(
            pane, 
            X_TEXT, y_origin-ROW2Y(2,FSZ),
            FSZ, str, SDL_LIGHT_BLUE, SDL_BLACK,
            SDL_EVENT_HPF_K1, SDL_EVENT_TYPE_MOUSE_WHEEL, pane_cx);

        sprintf(str, "%4.2f", hpf_k2);
        sdl_render_text_and_register_event(
            pane, 
            X_TEXT, y_origin-ROW2Y(1,FSZ),
            FSZ, str, SDL_LIGHT_BLUE, SDL_BLACK,
            SDL_EVENT_HPF_K2, SDL_EVENT_TYPE_MOUSE_WHEEL, pane_cx);

        return PANE_HANDLER_RET_NO_ACTION;
    }

    // -----------------------
    // -------- EVENT --------
    // -----------------------

    if (request == PANE_HANDLER_REQ_EVENT) {
        #define DELTA_VOL(x) (audio_out_volume[x] >= 10 ? 1.0 : 0.1)
        #define DELTA_PS(x)  (plot_scale[x] < 50 ? 1 : 10)

        switch (event->event_id) {
        // lpf / bpf filter params
        case SDL_EVENT_LPF_K1:
            if (event->mouse_wheel.delta_y > 0) lpf_k1++;
            if (event->mouse_wheel.delta_y < 0) lpf_k1--;
            break;
        case SDL_EVENT_LPF_K2:
            if (event->mouse_wheel.delta_y > 0) lpf_k2 += .01;
            if (event->mouse_wheel.delta_y < 0) lpf_k2 -= .01;
            break;

        // hpf / bpf filter params
        case SDL_EVENT_HPF_K1:
            if (event->mouse_wheel.delta_y > 0) hpf_k1++;
            if (event->mouse_wheel.delta_y < 0) hpf_k1--;
            break;
        case SDL_EVENT_HPF_K2:
            if (event->mouse_wheel.delta_y > 0) hpf_k2 += .01;
            if (event->mouse_wheel.delta_y < 0) hpf_k2 -= .01;
            break;

        // in data selection
        case SDL_EVENT_KEY_F(1) ... SDL_EVENT_KEY_F(2):
        case '1' ... '3':
            init_in_data(event->event_id);
            break;

        // audio out selection
        case SDL_EVENT_AUDIO_OUT_UNF:
            audio_out_filter = 0;
            break;
        case SDL_EVENT_AUDIO_OUT_LPF:
            audio_out_filter = 1;
            break;
        case SDL_EVENT_AUDIO_OUT_HPF:
            audio_out_filter = 2;
            break;
        case SDL_EVENT_AUDIO_OUT_BPF:
            audio_out_filter = 3;
            break;

        // manual volume ctrls
        case SDL_EVENT_UNF_VOLUME:
            if (event->mouse_wheel.delta_y > 0) audio_out_volume[0] += DELTA_VOL(0);
            if (event->mouse_wheel.delta_y < 0) audio_out_volume[0] -= DELTA_VOL(0);
            break;
        case SDL_EVENT_LPF_VOLUME:
            if (event->mouse_wheel.delta_y > 0) audio_out_volume[1] += DELTA_VOL(1);
            if (event->mouse_wheel.delta_y < 0) audio_out_volume[1] -= DELTA_VOL(1);
            break;
        case SDL_EVENT_HPF_VOLUME:
            if (event->mouse_wheel.delta_y > 0) audio_out_volume[2] += DELTA_VOL(2);
            if (event->mouse_wheel.delta_y < 0) audio_out_volume[2] -= DELTA_VOL(2);
            break;
        case SDL_EVENT_BPF_VOLUME:
            if (event->mouse_wheel.delta_y > 0) audio_out_volume[3] += DELTA_VOL(3);
            if (event->mouse_wheel.delta_y < 0) audio_out_volume[3] -= DELTA_VOL(3);
            break;
        case SDL_EVENT_KEY_UP_ARROW:
            if (audio_out_auto_volume == false) {
                audio_out_volume[audio_out_filter] += DELTA_VOL(audio_out_filter);
            }
            break;
        case SDL_EVENT_KEY_DOWN_ARROW:
            if (audio_out_auto_volume == false) {
                audio_out_volume[audio_out_filter] -= DELTA_VOL(audio_out_filter);
            }
            break;

        // toggle enable of auto volume adjust
        case 'a':
            audio_out_auto_volume = !audio_out_auto_volume;
            break;

        // set volume to 1 for all plots
        case 'A':
            audio_out_auto_volume = false;
            for (int i = 0; i < 4; i++) {
                audio_out_volume[i] = 1;
            }
            break;

        // plot scaling - xxx name audio_out_filter
        case SDL_EVENT_KEY_SHIFT_UP_ARROW:
            plot_scale[audio_out_filter] += DELTA_PS(audio_out_filter);
            break;
        case SDL_EVENT_KEY_SHIFT_DOWN_ARROW:
            plot_scale[audio_out_filter] -= DELTA_PS(audio_out_filter);
            break;
        case SDL_EVENT_PLOT_SCALE_UNF:
            if (event->mouse_wheel.delta_y > 0) plot_scale[0] += DELTA_PS(0);
            if (event->mouse_wheel.delta_y < 0) plot_scale[0] -= DELTA_PS(0);;
            break;
        case SDL_EVENT_PLOT_SCALE_LPF:
            if (event->mouse_wheel.delta_y > 0) plot_scale[1] += DELTA_PS(1);
            if (event->mouse_wheel.delta_y < 0) plot_scale[1] -= DELTA_PS(1);;
            break;
        case SDL_EVENT_PLOT_SCALE_HPF:
            if (event->mouse_wheel.delta_y > 0) plot_scale[2] += DELTA_PS(2);
            if (event->mouse_wheel.delta_y < 0) plot_scale[2] -= DELTA_PS(2);;
            break;
        case SDL_EVENT_PLOT_SCALE_BPF:
            if (event->mouse_wheel.delta_y > 0) plot_scale[3] += DELTA_PS(3);
            if (event->mouse_wheel.delta_y < 0) plot_scale[3] -= DELTA_PS(3);;
            break;

        // reset
        case 'r':
            reset_params();
            break;

        default:
            break;
        }

        // clip values that may have been changed by code above
        clip_int(&lpf_k1, 1, 500);
        clip_double(&lpf_k2, 0.0, 1.0);
        clip_int(&hpf_k1, 1, 500);
        clip_double(&hpf_k2, 0.0, 1.0);
        for (int i = 0; i < 4; i++) {
            clip_double(&audio_out_volume[i], 0, 1e6);
        }
        for (int i = 0; i < 4; i++) {
            clip_int(&plot_scale[i], 1, 1000);
        }

        return PANE_HANDLER_RET_NO_ACTION;
    }

    // ---------------------------
    // -------- TERMINATE --------
    // ---------------------------

    if (request == PANE_HANDLER_REQ_TERMINATE) {
        return PANE_HANDLER_RET_NO_ACTION;
    }

    // not reached
    printf("FATAL: not reached\n");
    exit(1);
    return PANE_HANDLER_RET_NO_ACTION;
}

static int plot(rect_t *pane, int idx, complex *data, int n)
{
    int y_pixels, y_max, y_origin, x_max, i, x;
    double freq, absv, y_values[4000];
    char str[100];

    static double max_y_value;

    #define MAX_PLOT_FREQ 5001

    // init
    y_pixels = pane->h / 4;
    y_origin = y_pixels * (idx + 1) - 20;
    y_max    = y_pixels - 20;
    x_max    = pane->w - 200;
    memset(y_values, 0, sizeof(y_values));
#if 0
    if (idx == 0) max_y_value = 0;
#else
    max_y_value = 0;
#endif

    // determine the y value that will be plotted below
    for (i = 0; i < n; i++) {
        freq = i * ((double)SAMPLE_RATE / n);
        if (freq > MAX_PLOT_FREQ) {
            break;
        }

        absv = cabs(data[i]);

        x = freq / MAX_PLOT_FREQ * x_max;

        if (absv > y_values[x]) {
            y_values[x] = absv;

#if 0
            // max_y_value is determined for plot idx 0, and is 
            // subsequently used for plot idx 1 to 3 the are next called
            if (idx == 0 && y_values[x] > max_y_value) {
                max_y_value = y_values[x];
            }
#else
            // max_y_values is determined for each plot
            if (y_values[x] > max_y_value) {
                max_y_value = y_values[x];
            }
#endif
        }
    }

    // plot y values
    if (max_y_value > 0) {
        for (x = 0; x < x_max; x++) {
            double v = (y_values[x] / max_y_value) * plot_scale[idx];
            if (v < .01) continue;
            if (v > 1) v = 1;
            sdl_render_line(pane, 
                            x, y_origin, 
                            x, y_origin - v * y_max,
                            SDL_WHITE);
        }
    }

    // x axis
    int color = (idx == audio_out_filter ? SDL_BLUE : SDL_GREEN);
    sdl_render_line(pane, 0, y_origin, x_max, y_origin, color);
    for (freq = 200; freq <= MAX_PLOT_FREQ-200; freq+= 200) {
        x = freq / MAX_PLOT_FREQ * x_max;
        sprintf(str, "%d", (int)(nearbyint(freq)/100));
        sdl_render_text(pane,
            x-COL2X(strlen(str),20)/2, y_origin+1, 20, str, color, SDL_BLACK);
    }

    // return the location of the y axis for this plot;
    // this is used by caller when displaying plot information text 
    // to the right of the plot
    return y_origin;
}

// -----------------  UTILS  -------------------------------------

static void apply_low_pass_filter(complex *data, int n)
{
    double cx[1000];
    memset(cx,0,sizeof(cx));
    for (int i = 0; i < n; i++) {
        data[i] = low_pass_filter_ex(creal(data[i]), cx, lpf_k1, lpf_k2);
    }
}

static void apply_high_pass_filter(complex *data, int n)
{
    double cx[1000];
    memset(cx,0,sizeof(cx));
    for (int i = 0; i < n; i++) {
        data[i] = high_pass_filter_ex(creal(data[i]), cx, hpf_k1, hpf_k2);
    }
}

static void apply_band_pass_filter(complex *data, int n)
{
    double cx[1000];
    memset(cx,0,sizeof(cx));
    for (int i = 0; i < n; i++) {
        data[i] = band_pass_filter_ex(creal(data[i]), cx, lpf_k1, lpf_k2, hpf_k1, hpf_k2);
    }
}

static void clip_int(int *v, int low, int high)
{
    if (*v < low) {
        *v = low;
    } else if (*v > high) {
        *v = high;
    }
}

static void clip_double(double *v, double low, double high)
{
    if (*v < low) {
        *v = low;
    } else if (*v > high) {
        *v = high;
    }
}

static int clip_meter(complex *data, int n, double vol, double *max_arg)
{
    int cnt=0, i;
    double max=0, v_clip=1/vol, v;

    for (i = 0; i < n; i++) {
        v = fabs(creal(data[i]));
        if (v > v_clip) cnt++;
        if (v > max) max = v;
    }

    *max_arg = max;

    return (cnt == 0     ? 0 :
            cnt < 10     ? 1 :
            cnt < 100    ? 2 :
            cnt < 1000   ? 3 :
            cnt < 10000  ? 4 :
            cnt < 100000 ? 5 :
                           6);
}
