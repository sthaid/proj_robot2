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

static void init_in_data(void);
static int pane_hndlr(pane_cx_t * pane_cx, int request, void * init_params, sdl_event_t * event);
static void plot(rect_t *pane, int idx, complex *data, char *title);
static void apply_low_pass_filter(complex *data, int n);

// -----------------  MAIN  --------------------------------------

int main(int argc, char **argv)
{
    // init the in_data array
    in_data  = (complex*) fftw_malloc(sizeof(complex) * N);
    init_in_data();

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
        1000000,          // 0=continuous, -1=never, else us   XXX was 10ms
        1,              // number of pane handler varargs that follow
        pane_hndlr, NULL, 0, 0, win_width, win_height, PANE_BORDER_STYLE_NONE);

    // clean up
    fftw_destroy_plan(plan);
    fftw_free(in); fftw_free(out);

    // terminate
    return 0;
}

static void init_in_data(void)
{
    double freq[] = { 100, 200, 300, 400, 500, 600, 700, 800, 900, 
                     1000, 1100, 1200, 1300, 1400, 1500 };
    int i, j;

    // XXX scale so transform is +/-1
    memset(in_data, 0, N*sizeof(complex));
    for (i = 0; i < sizeof(freq)/sizeof(freq[0]); i++) {
        double f = freq[i];
        for (j = 0; j < N; j++) {
            in_data[j] += 1.99 * sin((2*M_PI) * f * ((double)j/SAMPLE_RATE));
        }
    }
}

// -----------------  PANE HNDLR----------------------------------

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
        memcpy(in, in_data, N*sizeof(complex));
        fftw_execute(plan);
        plot(pane, 0, out, "TITLE");

        memcpy(in, in_data, N*sizeof(complex));
        apply_low_pass_filter(in, N);
        fftw_execute(plan);
        plot(pane, 1, out, "TITLE");

        memcpy(in, in_data, N*sizeof(complex));
        apply_low_pass_filter(in, N);
        apply_low_pass_filter(in, N);
        fftw_execute(plan);
        plot(pane, 2, out, "TITLE");

        memcpy(in, in_data, N*sizeof(complex));
        apply_low_pass_filter(in, N);
        apply_low_pass_filter(in, N);
        apply_low_pass_filter(in, N);
        fftw_execute(plan);
        plot(pane, 3, out, "TITLE");

        //plot(pane, 2, out, "TITLE");
        //plot(pane, 3, out, "TITLE");

        return PANE_HANDLER_RET_NO_ACTION;
    }

    // -----------------------
    // -------- EVENT --------
    // -----------------------

    if (request == PANE_HANDLER_REQ_EVENT) {
        switch (event->event_id) {
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

// xxx maybe data is not an arg
static void plot(rect_t *pane, int idx, complex *data, char *title)
{
    int y_pixels, y_max, y_origin, x_max, i, x;
    double freq, absv, y_values[5000];

    #define MAX_PLOT_FREQ 1600

    y_pixels = pane->h / 4;
    y_origin = y_pixels * (idx + 1) - 20;
    y_max    = y_pixels - 20;
    x_max    = pane->w - 100;

    memset(y_values, 0, sizeof(y_values));

    sdl_render_line(pane, 0, y_origin, pane->w-100, y_origin, SDL_GREEN);
    sdl_render_printf(pane, 
                      0, y_origin+1, 
                      20, SDL_WHITE, SDL_BLUE, "%s", title);


    for (i = 0; i < N; i++) {
        freq = i * ((double)SAMPLE_RATE / N);
        if (freq >= MAX_PLOT_FREQ) {
            break;
        }

        absv = cabs(data[i]) / N;
        if (absv > 1) {
            printf("warning: absv = %f\n", absv);
            absv = 1;
        }

        x = freq / MAX_PLOT_FREQ * x_max;

        if (absv > y_values[x]) {
            y_values[x] = absv;
        }
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
}

static void apply_low_pass_filter(complex *data, int n)
{
    double cx = 0;
    int i;

    for (i = 0; i < n; i++) {
        data[i] = low_pass_filter(creal(data[i]), &cx);
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
