// XXX 
// - x scale
// - f1,f2

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
#include <fftw3.h>

#include <filter_utils.h>
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

//
// prototypes
//

static void init_data_sin(complex *data, int n, int freq_start, int freq_end, int freq_incr);
static void init_data_rand(complex *data, int n);
static int pane_hndlr(pane_cx_t * pane_cx, int request, void * init_params, sdl_event_t * event);
static int plot(rect_t *pane, int idx, complex *data, int n, char *fmt, ...);
static void apply_low_pass_filter(complex *data, int n, int k1, double k2);
static void apply_high_pass_filter(complex *data, int n, int k1, double k2);
static void apply_band_pass_filter(complex *data, int n, int lpf_k1, double lpf_k2, int hpf_k1, double hpf_k2);
static void clip_int(int *v, int low, int high);
static void clip_double(double *v, double low, double high);

// -----------------  MAIN  --------------------------------------

int main(int argc, char **argv)
{
    char *data_type_str = "sin";

    // usage: filters [-t sin|rand]

    // parse options
    while (true) {
        int ch = getopt(argc, argv, "t:h");
        if (ch == -1) {
            break;
        }
        switch (ch) {
        case 't':
            data_type_str = optarg;
            break;
        case 'h':
            printf("usage: filters [-t sin|rand]\n");
            return 0;
        default:
            return 1;
        }
    }

    // init the in_data array
    in_data  = (complex*) fftw_malloc(sizeof(complex) * N);
    if (strcmp(data_type_str, "sin") == 0) {
        init_data_sin(in_data, N, 25, 1500, 25);
    } else if (strcmp(data_type_str, "rand") == 0) {
        init_data_rand(in_data, N);
    } else {
        printf("ERROR: invalid data_type_str '%s', expected 'sin' or 'rand'\n", data_type_str);
        return 1;
    }

    // allocate in and out arrays in create the plan
    in  = (complex*) fftw_malloc(sizeof(complex) * N);
    out = (complex*) fftw_malloc(sizeof(complex) * N);
    plan = fftw_plan_dft_1d(N, in, out, FFTW_FORWARD, FFTW_ESTIMATE);

    // init sdl
    if (sdl_init(&win_width, &win_height, opt_fullscreen, false, false) < 0) {
        printf("sdl_init %dx%d failed\n", win_width, win_height);
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

static void init_data_sin(complex *data, int n, int freq_start, int freq_end, int freq_incr)
{
    int i, f;

    memset(data, 0, n*sizeof(complex));
    for (f = freq_start; f <= freq_end; f += freq_incr) {
        for (i = 0; i < n; i++) {
            data[i] += sin((2*M_PI) * f * ((double)i/SAMPLE_RATE));
        }
    }
}

static void init_data_rand(complex *data, int n)
{
    int i, cnt;

    memset(data, 0, n*sizeof(complex));
    for (cnt = 0; cnt < 10; cnt++) {
        for (i = 0; i < n; i++) {
            data[i] += ((double)random() / RAND_MAX) - 0.5;
        }
    }
}

// -----------------  PANE HNDLR----------------------------------

static int pane_hndlr(pane_cx_t * pane_cx, int request, void * init_params, sdl_event_t * event)
{
    rect_t *pane = &pane_cx->pane;

    static int    lpf_k1 = 5;
    static double lpf_k2 = 0.95;
    static int    hpf_k1 = 5;
    static double hpf_k2 = 0.95;

    #define SDL_EVENT_LPF_K1   (SDL_EVENT_USER_DEFINED + 0)
    #define SDL_EVENT_LPF_K2   (SDL_EVENT_USER_DEFINED + 1)
    #define SDL_EVENT_HPF_K1   (SDL_EVENT_USER_DEFINED + 2)
    #define SDL_EVENT_HPF_K2   (SDL_EVENT_USER_DEFINED + 3)

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

        // plot fft of in_data
        memcpy(in, in_data, N*sizeof(complex));
        t1 = TIME(fftw_execute(plan));
        plot(pane, 0, out, N, "DATA SPECTRUM - FFT=%0.3f SECS", t1);

        // plot fft of low pass filtered in_data
        memcpy(in, in_data, N*sizeof(complex));
        t1 = TIME(apply_low_pass_filter(in, N, lpf_k1, lpf_k2));
        fftw_execute(plan);
        y_origin = plot(pane, 1, out, N, "LOW PASS FILTER SPECTRUM - LPF=%0.3f SECS", t1);

        sprintf(str, "%4d", lpf_k1);
        sdl_render_text_and_register_event(
            pane, 
            pane->w-95, y_origin-ROW2Y(3.0,30),
            30, str, SDL_LIGHT_BLUE, SDL_BLACK, 
            SDL_EVENT_LPF_K1, SDL_EVENT_TYPE_MOUSE_WHEEL, pane_cx);

        sprintf(str, "%4.2f", lpf_k2);
        sdl_render_text_and_register_event(
            pane, 
            pane->w-95, y_origin-ROW2Y(1.5,30),
            30, str, SDL_LIGHT_BLUE, SDL_BLACK, 
            SDL_EVENT_LPF_K2, SDL_EVENT_TYPE_MOUSE_WHEEL, pane_cx);

        // plot fft of high pass filtered in_data
        memcpy(in, in_data, N*sizeof(complex));
        t1 = TIME(apply_high_pass_filter(in, N, hpf_k1, hpf_k2));
        fftw_execute(plan);
        y_origin = plot(pane, 2, out, N, "HIGH PASS FILTER SPECTRUM - HPF=%0.3f SECS", t1);

        sprintf(str, "%4d", hpf_k1);
        sdl_render_text_and_register_event(
            pane, 
            pane->w-95, y_origin-ROW2Y(3.0,30),
            30, str, SDL_LIGHT_BLUE, SDL_BLACK, 
            SDL_EVENT_HPF_K1, SDL_EVENT_TYPE_MOUSE_WHEEL, pane_cx);

        sprintf(str, "%4.2f", hpf_k2);
        sdl_render_text_and_register_event(
            pane, 
            pane->w-95, y_origin-ROW2Y(1.5,30),
            30, str, SDL_LIGHT_BLUE, SDL_BLACK, 
            SDL_EVENT_HPF_K2, SDL_EVENT_TYPE_MOUSE_WHEEL, pane_cx);

        // plot fft of bndigh pass filtered in_data
        memcpy(in, in_data, N*sizeof(complex));
        t1 = TIME(apply_band_pass_filter(in, N, lpf_k1, lpf_k2, hpf_k1, hpf_k2));
        fftw_execute(plan);
        y_origin = plot(pane, 3, out, N, "BAND PASS FILTER SPECTRUM - BPF=%0.3f SECS", t1);

        sprintf(str, "%4d", lpf_k1);
        sdl_render_text_and_register_event(
            pane, 
            pane->w-95, y_origin-ROW2Y(6.0,30),
            30, str, SDL_LIGHT_BLUE, SDL_BLACK,
            SDL_EVENT_LPF_K1, SDL_EVENT_TYPE_MOUSE_WHEEL, pane_cx);

        sprintf(str, "%4.2f", lpf_k2);
        sdl_render_text_and_register_event(
            pane, 
            pane->w-95, y_origin-ROW2Y(4.5,30),
            30, str, SDL_LIGHT_BLUE, SDL_BLACK,
            SDL_EVENT_LPF_K2, SDL_EVENT_TYPE_MOUSE_WHEEL, pane_cx);

        sprintf(str, "%4d", hpf_k1);
        sdl_render_text_and_register_event(
            pane, 
            pane->w-95, y_origin-ROW2Y(3.0,30),
            30, str, SDL_LIGHT_BLUE, SDL_BLACK,
            SDL_EVENT_HPF_K1, SDL_EVENT_TYPE_MOUSE_WHEEL, pane_cx);

        sprintf(str, "%4.2f", hpf_k2);
        sdl_render_text_and_register_event(
            pane, 
            pane->w-95, y_origin-ROW2Y(1.5,30),
            30, str, SDL_LIGHT_BLUE, SDL_BLACK,
            SDL_EVENT_HPF_K2, SDL_EVENT_TYPE_MOUSE_WHEEL, pane_cx);

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
        case SDL_EVENT_KEY_F(1):
            init_data_sin(in_data, N, 25, 1500, 25);
            break;
        case SDL_EVENT_KEY_F(2):
            init_data_rand(in_data, N);
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
    printf("BUG: not reached\n");
    exit(1);
    return PANE_HANDLER_RET_NO_ACTION;
}

static int plot(rect_t *pane, int idx, complex *data, int n, char *fmt, ...)
{
    int y_pixels, y_max, y_origin, x_max, i, x;
    double freq, absv, y_values[5000], max_y_value=0;
    char title[100];
    va_list ap;

    #define MAX_PLOT_FREQ 1501

    y_pixels = pane->h / 4;
    y_origin = y_pixels * (idx + 1) - 20;
    y_max    = y_pixels - 20;
    x_max    = pane->w - 100;
    memset(y_values, 0, sizeof(y_values));

    va_start(ap, fmt);
    vsprintf(title, fmt, ap);
    va_end(ap);
    sdl_render_line(pane, 0, y_origin, x_max, y_origin, SDL_GREEN);
    sdl_render_printf(pane, 
                      0, y_origin+1, 
                      20, SDL_WHITE, SDL_BLACK, "%s", title);

    for (i = 0; i < n; i++) {
        freq = i * ((double)SAMPLE_RATE / n);
        if (freq > MAX_PLOT_FREQ) {
            break;
        }

        absv = cabs(data[i]);

        x = freq / MAX_PLOT_FREQ * x_max;

        if (absv > y_values[x]) {
            y_values[x] = absv;
            if (y_values[x] > max_y_value) {
                max_y_value = y_values[x];
            }
        }
    }

    if (max_y_value == 0) {
        return y_origin;
    }

    for (x = 0; x < x_max; x++) {
        y_values[x] *= (1 / max_y_value);
    }
        
    for (x = 0; x < x_max; x++) {
        if (y_values[x] < .01) {
            continue;
        }
        sdl_render_line(pane, 
                        x, y_origin, 
                        x, y_origin - y_values[x] * y_max,
                        SDL_WHITE);
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

