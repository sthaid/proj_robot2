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

#if 0
yyy print in_data src string
#endif


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

#include <filter_utils.h>
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

static int         win_width = 1500;
static int         win_height = 800;
static int         opt_fullscreen = false;

static complex    *in_data;
static complex    *in;
static complex    *out;
static fftw_plan   plan;

#if 0
static int         lpf_k1 = 7;
static double      lpf_k2 = 0.95;
static int         hpf_k1 = 7;
static double      hpf_k2 = 0.50;
#else
static int         lpf_k1 = 5;
static double      lpf_k2 = 0.95;
static int         hpf_k1 = 5;
static double      hpf_k2 = 0.95;
#endif

static char  *file_name;
static float *file_data;
static int    file_max_chan;
static int    file_max_data;
static int    file_sample_rate;

static char       *audio_out_dev = DEFAULT_OUTPUT_DEVICE;
static int         audio_out_filter = 0;

//
// prototypes
//

static void init_in_data(int type);
static void *audio_out_thread(void *cx);
static int audio_out_get_frame(float *data, void *cx);
static int pane_hndlr(pane_cx_t * pane_cx, int request, void * init_params, sdl_event_t * event);
static int plot(rect_t *pane, int idx, complex *data, int n);
static void apply_low_pass_filter(complex *data, int n, int k1, double k2);
static void apply_high_pass_filter(complex *data, int n, int k1, double k2);
static void apply_band_pass_filter(complex *data, int n, int lpf_k1, double lpf_k2, int hpf_k1, double hpf_k2);
static void clip_int(int *v, int low, int high);
static void clip_double(double *v, double low, double high);

// -----------------  MAIN  --------------------------------------

int main(int argc, char **argv)
{
    pthread_t tid;

    #define USAGE \
    "usage: filters [-f file_name.wav] [-d out_dev] -h"  // yyy better usage comment

    setlinebuf(stdout);

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

    // yyy print args
    printf("INFO: audio_out_dev=%s file_name=%s\n", audio_out_dev, file_name);

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

    // init data, use white noise ('1') as default
    in_data  = (complex*)fftw_malloc(sizeof(complex) * N);
    init_in_data('1');

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

// -----------------  INIT IN DATA ARRAY  ------------------------

// arg: 'type'
// - F1 ... F12:  tones 100 ... 1200 Hz
// - '1':         white noise
// - '2':         mix of sine waves
// - '3':         file-data
static void init_in_data(int type)
{
    int i, j, freq;

    // zero the in_data array
    memset(in_data, 0, N*sizeof(complex));

    // fill the in_data array, based on 'type' arg
    switch (type) {
    case SDL_EVENT_KEY_F(1) ... SDL_EVENT_KEY_F(12):  // tone
        freq = (type - SDL_EVENT_KEY_F(1) + 1) * 100.;
        for (i = 0; i < N; i++) {
            in_data[i] = sin((2*M_PI) * freq * ((double)i/SAMPLE_RATE));
        }
        break;
    case '1':  // white nosie
        for (i = 0; i < N; i++) {
            in_data[i] = ((double)random() / RAND_MAX) - 0.5;
        }
        break;
    case '2':  // sum of sine waves
        for (freq = 25; freq <= 1500; freq += 25) {
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
        for (i=0, j=0; i < N; i++) {
            in_data[i] = file_data[j];
            j += file_max_chan;
            if (j >= file_max_data) j = 0;
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
    if (pa_play2(audio_out_dev, 1, SAMPLE_RATE, audio_out_get_frame, NULL) < 0) {
        printf("FATAL: pa_play2 failed\n");
        exit(1); 
    }
    return NULL;
}

static int audio_out_get_frame(float *out_data, void *cx_arg)
{
    double vd, vo;

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

    // return the filtered value
    out_data[0] = vo;

    // continue 
    return 0;
}

// -----------------  PANE HNDLR----------------------------------

static int pane_hndlr(pane_cx_t * pane_cx, int request, void * init_params, sdl_event_t * event)
{
    rect_t *pane = &pane_cx->pane;

    #define SDL_EVENT_LPF_K1       (SDL_EVENT_USER_DEFINED + 0)
    #define SDL_EVENT_LPF_K2       (SDL_EVENT_USER_DEFINED + 1)
    #define SDL_EVENT_HPF_K1       (SDL_EVENT_USER_DEFINED + 2)
    #define SDL_EVENT_HPF_K2       (SDL_EVENT_USER_DEFINED + 3)

    #define SDL_EVENT_AUDIO_OUT_NO_FLT (SDL_EVENT_USER_DEFINED + 10)
    #define SDL_EVENT_AUDIO_OUT_LPF    (SDL_EVENT_USER_DEFINED + 11)
    #define SDL_EVENT_AUDIO_OUT_HPF    (SDL_EVENT_USER_DEFINED + 12)
    #define SDL_EVENT_AUDIO_OUT_BPF    (SDL_EVENT_USER_DEFINED + 13)

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

        // ----------------------
        // plot fft of 'in' data
        // ----------------------
        memcpy(in, in_data, N*sizeof(complex));
        t1 = TIME(fftw_execute(plan));
        y_origin = plot(pane, 0, out, N);

        sdl_render_printf(pane, pane->w-95, y_origin-ROW2Y(5,30), 30, SDL_WHITE, SDL_BLACK, "NO_FLT");

        sdl_render_printf(pane, pane->w-95, y_origin-ROW2Y(4,30), 30, SDL_WHITE, SDL_BLACK, "%0.3f", t1);

        sdl_register_event(pane, &(rect_t){0, y_origin-180, 1400, 180 },
                           SDL_EVENT_AUDIO_OUT_NO_FLT, SDL_EVENT_TYPE_MOUSE_CLICK, pane_cx);

        // -------------------------------------
        // plot fft of low pass filtered 'in' data
        // -------------------------------------
        memcpy(in, in_data, N*sizeof(complex));
        t1 = TIME(apply_low_pass_filter(in, N, lpf_k1, lpf_k2));
        fftw_execute(plan);
        y_origin = plot(pane, 1, out, N);

        sdl_render_printf(pane, pane->w-95, y_origin-ROW2Y(5,30), 30, SDL_WHITE, SDL_BLACK, "LPF");

        sdl_render_printf(pane, pane->w-95, y_origin-ROW2Y(4,30), 30, SDL_WHITE, SDL_BLACK, "%0.3f", t1);

        sprintf(str, "%4d", lpf_k1);
        sdl_render_text_and_register_event(
            pane, 
            pane->w-95, y_origin-ROW2Y(2,30),
            30, str, SDL_LIGHT_BLUE, SDL_BLACK, 
            SDL_EVENT_LPF_K1, SDL_EVENT_TYPE_MOUSE_WHEEL, pane_cx);

        sprintf(str, "%4.2f", lpf_k2);
        sdl_render_text_and_register_event(
            pane, 
            pane->w-95, y_origin-ROW2Y(1,30),
            30, str, SDL_LIGHT_BLUE, SDL_BLACK, 
            SDL_EVENT_LPF_K2, SDL_EVENT_TYPE_MOUSE_WHEEL, pane_cx);

        sdl_register_event(pane, &(rect_t){0, y_origin-180, 1400, 180 },
                           SDL_EVENT_AUDIO_OUT_LPF, SDL_EVENT_TYPE_MOUSE_CLICK, pane_cx);

        // -------------------------------------
        // plot fft of high pass filtered 'in' data
        // -------------------------------------
        memcpy(in, in_data, N*sizeof(complex));
        t1 = TIME(apply_high_pass_filter(in, N, hpf_k1, hpf_k2));
        fftw_execute(plan);
        y_origin = plot(pane, 2, out, N);

        sdl_render_printf(pane, pane->w-95, y_origin-ROW2Y(5,30), 30, SDL_WHITE, SDL_BLACK, "HPF");

        sdl_render_printf(pane, pane->w-95, y_origin-ROW2Y(4,30), 30, SDL_WHITE, SDL_BLACK, "%0.3f", t1);

        sprintf(str, "%4d", hpf_k1);
        sdl_render_text_and_register_event(
            pane, 
            pane->w-95, y_origin-ROW2Y(2,30),
            30, str, SDL_LIGHT_BLUE, SDL_BLACK, 
            SDL_EVENT_HPF_K1, SDL_EVENT_TYPE_MOUSE_WHEEL, pane_cx);

        sprintf(str, "%4.2f", hpf_k2);
        sdl_render_text_and_register_event(
            pane, 
            pane->w-95, y_origin-ROW2Y(1,30),
            30, str, SDL_LIGHT_BLUE, SDL_BLACK, 
            SDL_EVENT_HPF_K2, SDL_EVENT_TYPE_MOUSE_WHEEL, pane_cx);

        sdl_register_event(pane, &(rect_t){0, y_origin-180, 1400, 180 },
                           SDL_EVENT_AUDIO_OUT_HPF, SDL_EVENT_TYPE_MOUSE_CLICK, pane_cx);

        // -------------------------------------
        // plot fft of band pass filtered 'in' data
        // -------------------------------------
        memcpy(in, in_data, N*sizeof(complex));
        t1 = TIME(apply_band_pass_filter(in, N, lpf_k1, lpf_k2, hpf_k1, hpf_k2));
        fftw_execute(plan);
        y_origin = plot(pane, 3, out, N);

        sdl_render_printf(pane, pane->w-95, y_origin-ROW2Y(6,30), 30, SDL_WHITE, SDL_BLACK, "BPF");

        sdl_render_printf(pane, pane->w-95, y_origin-ROW2Y(5,30), 30, SDL_WHITE, SDL_BLACK, "%0.3f", t1);

        sprintf(str, "%4d", lpf_k1);
        sdl_render_text_and_register_event(
            pane, 
            pane->w-95, y_origin-ROW2Y(4,30),
            30, str, SDL_LIGHT_BLUE, SDL_BLACK,
            SDL_EVENT_LPF_K1, SDL_EVENT_TYPE_MOUSE_WHEEL, pane_cx);

        sprintf(str, "%4.2f", lpf_k2);
        sdl_render_text_and_register_event(
            pane, 
            pane->w-95, y_origin-ROW2Y(3,30),
            30, str, SDL_LIGHT_BLUE, SDL_BLACK,
            SDL_EVENT_LPF_K2, SDL_EVENT_TYPE_MOUSE_WHEEL, pane_cx);

        sprintf(str, "%4d", hpf_k1);
        sdl_render_text_and_register_event(
            pane, 
            pane->w-95, y_origin-ROW2Y(2,30),
            30, str, SDL_LIGHT_BLUE, SDL_BLACK,
            SDL_EVENT_HPF_K1, SDL_EVENT_TYPE_MOUSE_WHEEL, pane_cx);

        sprintf(str, "%4.2f", hpf_k2);
        sdl_render_text_and_register_event(
            pane, 
            pane->w-95, y_origin-ROW2Y(1,30),
            30, str, SDL_LIGHT_BLUE, SDL_BLACK,
            SDL_EVENT_HPF_K2, SDL_EVENT_TYPE_MOUSE_WHEEL, pane_cx);

        sdl_register_event(pane, &(rect_t){0, y_origin-180, 1400, 180 },
                           SDL_EVENT_AUDIO_OUT_BPF, SDL_EVENT_TYPE_MOUSE_CLICK, pane_cx);

        return PANE_HANDLER_RET_NO_ACTION;
    }

    // -----------------------
    // -------- EVENT --------
    // -----------------------

    if (request == PANE_HANDLER_REQ_EVENT) {
        switch (event->event_id) {
        case SDL_EVENT_LPF_K1:
            if (event->mouse_wheel.delta_y > 0) lpf_k1++;
            if (event->mouse_wheel.delta_y < 0) lpf_k1--;
            clip_int(&lpf_k1, 1, 500);
            break;
        case SDL_EVENT_LPF_K2:
            if (event->mouse_wheel.delta_y > 0) lpf_k2 += .01;
            if (event->mouse_wheel.delta_y < 0) lpf_k2 -= .01;
            clip_double(&lpf_k2, 0.0, 1.0);
            break;

        case SDL_EVENT_HPF_K1:
            if (event->mouse_wheel.delta_y > 0) hpf_k1++;
            if (event->mouse_wheel.delta_y < 0) hpf_k1--;
            clip_int(&hpf_k1, 1, 500);
            break;
        case SDL_EVENT_HPF_K2:
            if (event->mouse_wheel.delta_y > 0) hpf_k2 += .01;
            if (event->mouse_wheel.delta_y < 0) hpf_k2 -= .01;
            clip_double(&hpf_k2, 0.0, 1.0);
            break;

// xxx audio_out_filter
        case SDL_EVENT_KEY_F(1) ... SDL_EVENT_KEY_F(12):
        case '1' ... '3':
            init_in_data(event->event_id);
            break;

        case SDL_EVENT_AUDIO_OUT_NO_FLT:
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

        default:
            break;
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

    #define MAX_PLOT_FREQ 1501

    y_pixels = pane->h / 4;
    y_origin = y_pixels * (idx + 1) - 20;
    y_max    = y_pixels - 20;
    x_max    = pane->w - 100;
    memset(y_values, 0, sizeof(y_values));
    if (idx == 0) max_y_value = 0;

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
            if (idx == 0 && y_values[x] > max_y_value) {
                max_y_value = y_values[x];
            }
        }
    }

    if (idx == 0) printf("max_y_value = %0.3f\n", max_y_value);

    // plot y values
    if (max_y_value > 0) {
        for (x = 0; x < x_max; x++) {
            double v = y_values[x] / max_y_value;
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
    for (freq = 100; freq <= MAX_PLOT_FREQ-100; freq+= 100) {
        x = freq / MAX_PLOT_FREQ * x_max;
        sprintf(str, "%d", (int)nearbyint(freq));
        sdl_render_text(pane,
            x-COL2X(strlen(str),20)/2, y_origin+1, 20, str, color, SDL_BLACK);
    }

    return y_origin;
}

static void apply_low_pass_filter(complex *data, int n, int k1, double k2)
{
    double cx[1000];
    memset(cx,0,sizeof(cx));
    for (int i = 0; i < n; i++) {
        data[i] = low_pass_filter_ex(creal(data[i]), cx, k1, k2);
    }
}

static void apply_high_pass_filter(complex *data, int n, int k1, double k2)
{
    double cx[1000];
    memset(cx,0,sizeof(cx));
    for (int i = 0; i < n; i++) {
        data[i] = high_pass_filter_ex(creal(data[i]), cx, k1, k2);
    }
}

static void apply_band_pass_filter(complex *data, int n, int lpf_k1, double lpf_k2, int hpf_k1, double hpf_k2)
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

