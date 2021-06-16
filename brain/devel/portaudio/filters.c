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
#include <stdlib.h>
#include <string.h>
#include <complex.h>
#include <math.h>
#include <fftw3.h>

#define SAMPLE_RATE 48000  // samples per sec
#define DURATION    5      // secs
#define N           (DURATION * SAMPLE_RATE)

static void filter_in(complex *in, int n);
static void init_in(complex *in, int n);
static void print_out(complex *out, int n);

int main(int argc, char **argv)
{
    fftw_complex *in, *out;
    fftw_plan p;

    // allocate in and out arrays in create the plan
    printf("allocate...\n");
    in = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * N);
    out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * N);
    printf("create plan...\n");
    p = fftw_plan_dft_1d(N, in, out, FFTW_FORWARD, FFTW_ESTIMATE);

    // init the in array
    printf("init in...\n");
    init_in(in, N);

#if 1
    // filter the in array
    printf("filter in...\n");
    filter_in(in, N);
#endif

    // execute the transform
    printf("execute the tranform...\n");
    fftw_execute(p);

    printf("print results...\n");
    print_out(out, N);

    // clean up
    printf("clean up...\n");
    fftw_destroy_plan(p);
    fftw_free(in); fftw_free(out);

    // terminate
    return 0;
}

// ------------------------------------------------------------------------

// XXX todo
// - plot
// - white noise, random data
// - specifiy cutoff frequency
// - include theses filters in the analyze pgm

static double low_pass_filter(double v, double *cx)
{
    *cx = 0.95 * *cx + 0.05 * v;
    return *cx;
}

static double high_pass_filter(double v, double *cx)
{
    *cx = *cx * .95 + .05 * v;
    return v - *cx;
}

static void filter_in(complex *in, int n)
{
    double hpf_cx = 0;
    double lpf_cx = 0;
    int i;

    for (i = 0; i < n; i++) {
        double v = in[i];
        v = low_pass_filter(v, &lpf_cx);
        //v = high_pass_filter(v, &hpf_cx);
        in[i] = v;
    }
}

// ------------------------------------------------------------------------

static void init_in(complex *in, int n)
{
#if 0
    double freq[] = { 100, 300, 500, 700, 900, 1100, 1300, 1500, 1700, 1900 };
    int i, j;

    memset(in, 0, n*sizeof(complex));

    for (i = 0; i < sizeof(freq)/sizeof(freq[0]); i++) {
        double f = freq[i];
        for (j = 0; j < n; j++) {
            in[j] += sin((2*M_PI) * f * ((double)j/SAMPLE_RATE));
        }
    }
#else
    int i;

    for (i = 0; i < n; i++) {
        in[i] = 2 * ((double)random() / RAND_MAX) - 1.0;
    }
#endif
}

// ------------------------------------------------------------------------

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
