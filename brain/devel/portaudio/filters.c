// XXX todo
// - plot
// - white noise, random data
// - specifiy cutoff frequency
// - include theses filters in the analyze pgm

#if 0
http://www.fftw.org/
http://www.fftw.org/fftw3.pdf

sudo apt install libfftw3-dev

Size that is th products of small factors transform more efficiently.

You must create the plan before initializing the input, because FFTW_MEASURE 
overwrites the in/out arrays.

The DFT results are stored in-order in the array out, with the zero-frequency (DC) 
component in out[0]. If in != out, the transform is out-of-place and the input array 
in is not modified.
#endif

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <complex.h>
#include <math.h>
#include <fftw3.h>

#include <filter_utils.h>
#include <util_sdl.h>

//
// defines
//

#define SAMPLE_RATE 48000  // samples per sec
#define DURATION    5      // secs
#define N           (DURATION * SAMPLE_RATE)

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
static int pane_hndlr(pane_cx_t * pane_cx, int request, void * init_params, sdl_event_t * event);
static int plot(rect_t *pane, int idx, complex *data, char *title);
static void apply_low_pass_filter(complex *data, int n, int k1, double k2);
static void apply_high_pass_filter(complex *data, int n, int k1, double k2);
static void apply_band_pass_filter(complex *data, int n, int k1lpf, double k2lpf, int k1hpf, double k2hpf);
static void clip_int(int *v, int low, int high);
static void clip_double(double *v, double low, double high);

// -----------------  MAIN  --------------------------------------

int main(int argc, char **argv)
{
    // init the in_data array
    in_data  = (complex*) fftw_malloc(sizeof(complex) * N);
    init_data_sin(in_data, N, 25, 1500, 25);

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

    memset(in_data, 0, N*sizeof(complex));
    for (f = freq_start; f <= freq_end; f += freq_incr) {
        printf("%d\n", f);
        for (i = 0; i < n; i++) {
            data[i] += sin((2*M_PI) * f * ((double)i/SAMPLE_RATE));
        }
    }
}

// -----------------  PANE HNDLR----------------------------------

static int pane_hndlr(pane_cx_t * pane_cx, int request, void * init_params, sdl_event_t * event)
{
    rect_t *pane = &pane_cx->pane;

    static int    lpf_k1 = 1;
    static double lpf_k2 = 0.95;
    static int    hpf_k1 = 1;
    static double hpf_k2 = 0.95;

    //static int    bpf_k1 = 1;
    //static double bpf_k2 = 0.95;
    //static int    bpf_k3 = 1;
    //static double bpf_k4 = 0.95;

    #define SDL_EVENT_LPF_K1   (SDL_EVENT_USER_DEFINED + 0)
    #define SDL_EVENT_LPF_K2   (SDL_EVENT_USER_DEFINED + 1)
    #define SDL_EVENT_HPF_K1   (SDL_EVENT_USER_DEFINED + 2)
    #define SDL_EVENT_HPF_K2   (SDL_EVENT_USER_DEFINED + 3)
#if 0
    #define SDL_EVENT_BPF_K1   (SDL_EVENT_USER_DEFINED + 4)
    #define SDL_EVENT_BPF_K2   (SDL_EVENT_USER_DEFINED + 5)
    #define SDL_EVENT_BPF_K3   (SDL_EVENT_USER_DEFINED + 6)
    #define SDL_EVENT_BPF_K4   (SDL_EVENT_USER_DEFINED + 7)
#endif

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

        // plot fft of in_data
        memcpy(in, in_data, N*sizeof(complex));
        fftw_execute(plan);
        plot(pane, 0, out, "DATA SPECTRUM");

        // plot fft of low pass filtered in_data
        memcpy(in, in_data, N*sizeof(complex));
        apply_low_pass_filter(in, N, lpf_k1, lpf_k2);
        fftw_execute(plan);
        y_origin = plot(pane, 1, out, "LOW PASS FILTER SPECTRUM");

        sprintf(str, "%5d", lpf_k1);
        sdl_render_text_and_register_event(
            pane, 
            pane->w-100, y_origin-ROW2Y(3.0,30),
            30, str, SDL_LIGHT_BLUE, SDL_BLACK, 
            SDL_EVENT_LPF_K1, SDL_EVENT_TYPE_MOUSE_WHEEL, pane_cx);

        sprintf(str, "%5.3f", lpf_k2);
        sdl_render_text_and_register_event(
            pane, 
            pane->w-100, y_origin-ROW2Y(1.5,30),
            30, str, SDL_LIGHT_BLUE, SDL_BLACK, 
            SDL_EVENT_LPF_K2, SDL_EVENT_TYPE_MOUSE_WHEEL, pane_cx);

        // plot fft of high pass filtered in_data
        memcpy(in, in_data, N*sizeof(complex));
        apply_high_pass_filter(in, N, hpf_k1, hpf_k2);
        fftw_execute(plan);
        y_origin = plot(pane, 2, out, "HIGH PASS FILTER SPECTRUM");

        sprintf(str, "%5d", hpf_k1);
        sdl_render_text_and_register_event(
            pane, 
            pane->w-100, y_origin-ROW2Y(3.0,30),
            30, str, SDL_LIGHT_BLUE, SDL_BLACK, 
            SDL_EVENT_HPF_K1, SDL_EVENT_TYPE_MOUSE_WHEEL, pane_cx);

        sprintf(str, "%5.3f", hpf_k2);
        sdl_render_text_and_register_event(
            pane, 
            pane->w-100, y_origin-ROW2Y(1.5,30),
            30, str, SDL_LIGHT_BLUE, SDL_BLACK, 
            SDL_EVENT_HPF_K2, SDL_EVENT_TYPE_MOUSE_WHEEL, pane_cx);

        // plot fft of bndigh pass filtered in_data
        memcpy(in, in_data, N*sizeof(complex));
        apply_band_pass_filter(in, N, lpf_k1, lpf_k2, hpf_k1, hpf_k2);
        fftw_execute(plan);
        y_origin = plot(pane, 3, out, "BAND PASS FILTER SPECTRUM");

#if 0
        sprintf(str, "%5d", bpf_k1);
        sdl_render_text_and_register_event(
            pane, 
            pane->w-100, y_origin-ROW2Y(6.0,30),
            30, str, SDL_LIGHT_BLUE, SDL_BLACK, 
            SDL_EVENT_BPF_K1, SDL_EVENT_TYPE_MOUSE_WHEEL, pane_cx);

        sprintf(str, "%5.3f", bpf_k2);
        sdl_render_text_and_register_event(
            pane, 
            pane->w-100, y_origin-ROW2Y(4.5,30),
            30, str, SDL_LIGHT_BLUE, SDL_BLACK, 
            SDL_EVENT_BPF_K2, SDL_EVENT_TYPE_MOUSE_WHEEL, pane_cx);

        sprintf(str, "%5d", bpf_k3);
        sdl_render_text_and_register_event(
            pane, 
            pane->w-100, y_origin-ROW2Y(3.0,30),
            30, str, SDL_LIGHT_BLUE, SDL_BLACK, 
            SDL_EVENT_BPF_K3, SDL_EVENT_TYPE_MOUSE_WHEEL, pane_cx);

        sprintf(str, "%5.3f", bpf_k4);
        sdl_render_text_and_register_event(
            pane, 
            pane->w-100, y_origin-ROW2Y(1.5,30),
            30, str, SDL_LIGHT_BLUE, SDL_BLACK, 
            SDL_EVENT_BPF_K4, SDL_EVENT_TYPE_MOUSE_WHEEL, pane_cx);
#endif

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
            clip_int(&lpf_k1, 1, 30);
            break;
        case SDL_EVENT_LPF_K2:
            if (event->mouse_wheel.delta_y > 0) lpf_k2 += .01;
            if (event->mouse_wheel.delta_y < 0) lpf_k2 -= .01;
            clip_double(&lpf_k2, 0.5, 0.99);
            break;

        case SDL_EVENT_HPF_K1:
            if (event->mouse_wheel.delta_y > 0) hpf_k1++;
            if (event->mouse_wheel.delta_y < 0) hpf_k1--;
            clip_int(&hpf_k1, 1, 30);
            break;
        case SDL_EVENT_HPF_K2:
            if (event->mouse_wheel.delta_y > 0) hpf_k2 += .01;
            if (event->mouse_wheel.delta_y < 0) hpf_k2 -= .01;
            clip_double(&hpf_k2, 0.5, 0.99);
            break;

#if 0
//xxx use same constants, and boost amplitude of bandpass
        case SDL_EVENT_BPF_K1:
            if (event->mouse_wheel.delta_y > 0) bpf_k1++;
            if (event->mouse_wheel.delta_y < 0) bpf_k1--;
            clip_int(&bpf_k1, 1, 10);
            break;
        case SDL_EVENT_BPF_K2:
            if (event->mouse_wheel.delta_y > 0) bpf_k2 += .01;
            if (event->mouse_wheel.delta_y < 0) bpf_k2 -= .01;
            clip_double(&bpf_k2, 0.5, 0.99);
            break;
        case SDL_EVENT_BPF_K3:
            if (event->mouse_wheel.delta_y > 0) bpf_k3++;
            if (event->mouse_wheel.delta_y < 0) bpf_k3--;
            clip_int(&bpf_k3, 1, 10);
            break;
        case SDL_EVENT_BPF_K4:
            if (event->mouse_wheel.delta_y > 0) bpf_k4 += .01;
            if (event->mouse_wheel.delta_y < 0) bpf_k4 -= .01;
            clip_double(&bpf_k4, 0.5, 0.99);
            break;
#endif
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

// xxx maybe data is not an arg, just alwasy plot out
// xxx or else N should be an arg too
static int plot(rect_t *pane, int idx, complex *data, char *title)
{
    int y_pixels, y_max, y_origin, x_max, i, x;
    double freq, absv, y_values[5000], max_y_value=0;

    #define MAX_PLOT_FREQ 1505

    y_pixels = pane->h / 4;
    y_origin = y_pixels * (idx + 1) - 20;
    y_max    = y_pixels - 20;
    x_max    = pane->w - 100;

    memset(y_values, 0, sizeof(y_values));

    sdl_render_line(pane, 0, y_origin, x_max, y_origin, SDL_GREEN);
    sdl_render_printf(pane, 
                      0, y_origin+1, 
                      20, SDL_WHITE, SDL_BLACK, "%s", title);


    for (i = 0; i < N; i++) {
        freq = i * ((double)SAMPLE_RATE / N);
        if (freq > MAX_PLOT_FREQ) {
            break;
        }

#if 0
        absv = cabs(data[i]) / N;
        if (absv > 1) {
            printf("warning: absv = %f\n", absv);
            absv = 1;
        }
#else
        absv = cabs(data[i]);
#endif

        x = freq / MAX_PLOT_FREQ * x_max;

        if (absv > y_values[x]) {
            y_values[x] = absv;

            if (y_values[x] > max_y_value) {
                max_y_value = y_values[x];
            }
        }
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

// xxx xmaybe data and n not args
static void apply_low_pass_filter(complex *data, int n, int k1, double k2)
{
    double cx[100];
    memset(cx,0,sizeof(cx));
    for (int i = 0; i < n; i++) {
        data[i] = low_pass_filter_ex(creal(data[i]), cx, k1, k2);
    }
}

static void apply_high_pass_filter(complex *data, int n, int k1, double k2)
{
    double cx[100];
    memset(cx,0,sizeof(cx));
    for (int i = 0; i < n; i++) {
        data[i] = high_pass_filter_ex(creal(data[i]), cx, k1, k2);
    }
}

static void apply_band_pass_filter(complex *data, int n, int k1, double k2, int k3, double k4)
{
    double cx[100];
    memset(cx,0,sizeof(cx));
    for (int i = 0; i < n; i++) {
        data[i] = band_pass_filter_ex(creal(data[i]), cx, k1, k2, k3, k4);
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

#if 0
// ------------------------------------------------------------------------

    // xxx
    memcpy(in, in_data, N*sizeof(complex));
    apply_low_pass_filter(in, N, lpf_param);
    fftw_execute(p);
    plot(1, out, 0, 1600);
    register mouse wheel event for lpf_param

static void print_out(complex *out, int n)
{
    int i;

    n /= 2;

// XXX scale the output to magnitude range 0 - 1 ??
    for (i = 0; i < n; i++) {
        if (cabs(out[i]) / N > .001) {  // XXX was .01
            printf("%d: %f %f  - freq = %0.1f avg_val = %0.6f\n", 
                i,
                cabs(out[i]), 
                carg(out[i]) * (180./M_PI),
                i * ((double)SAMPLE_RATE / N),
                cabs(out[i]) / N);
        }
    }
}
#if 0
//static void print_out(complex *out, int n);
    // filter the in array
    printf("filter in...\n");
    double hpf_cx = 0;
    double lpf_cx = 0;
    for (int i = 0; i < N; i++) {
        in[i] = low_pass_filter(in[i], &lpf_cx);
    }

    // execute the transform
    printf("execute the tranform...\n");
    fftw_execute(p);

    printf("print results...\n");
    print_out(out, N);
#endif
#endif
