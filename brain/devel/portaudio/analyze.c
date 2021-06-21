#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>

#include <util_misc.h>

//
// defines
//

#define SAMPLE_RATE  40000
#define MAX_SAMPLES  48000

#define MAX_CHAN 4

#define MIN_AMP 1  // xxx

//
// typedefs
//

typedef struct {
    int    idx;
    int    max;
    double sum;
    double values[0];
} average_cx_t;

//
// variables
//

//
// prototpes
//

static void *get_data_from_file_thread(void *cx);
static void process_data(double *raw_data);

static double average(double v, average_cx_t *cx);
static average_cx_t *average_alloc_cx(int max);

// -------------------------------------------------------------------------

int main(int argc, char **argv)
{
    pthread_t tid;

    // create get_data_from_file_thread
    pthread_create(&tid, NULL, get_data_from_file_thread, NULL);

    // pause forever
    while (true) {
        pause();
    }

    // done
    return 0;
}

static void *get_data_from_file_thread(void *cx)
{
    // read file

    // loop, passing file data to routine for procesing

    return NULL;
}

// -------------------------------------------------------------------------

static void process_data(double *raw_data)
{
    int      i;
    uint64_t time_now;
    double   amp;

    static unsigned char lfd_idx;
    static double        filtered_data[MAX_CHAN][256];
    static average_cx_t *avg_amp_cx;
    static uint64_t      time_last;
    static bool          first_call = true;

    // init on first call
    if (first_call) {
        avg_amp_cx = average_alloc_cx(MAX_SAMPLES);
        first_call = false;
    }

    // print sample rate

#if 0
    // filter data 
    for (i = 0; i < MAX_CHAN; i++) {
        filtered_data[chan][lfd_idx] = low_pass_filter_ex(raw_data[chan], filter_cx[chan], LPF_K1, LPF_K2);
    }

    // correlate over 1 sec
    for (i = 0; i < MAX; i++) {
        corr_02[i] = correlate(filtered_data[0][-(MAX/2)], filtered_data[2][i+1-MAX], corr_02_cx);
        corr_13[i] = correlate(filtered_data[1][-(MAX/2)], filtered_data[3][i+1-MAX], corr_13_cx);
    }
#endif

    // average the amplitude over 1 sec, for chan 0;
    // if amplitude too low then return
    amp = average(filtered_data[0][lfd_idx], avg_amp_cx);
    if (amp < MIN_AMP) {
        goto done;
    }

    // if sound direction was last determined less than 1 second ago then return
    time_now = microsec_timer();
    if (time_now - time_last < 1000000) {
        goto done;
    }
    time_last = time_now;

    // find the peak correlation

    // compute the sound direction

    // debug printint?


done:
    lfd_idx++;
}

// -------------------------------------------------------------------

static double average(double v, average_cx_t *cx)
{
    double tmp = v * v;

    cx->sum += (tmp - cx->values[cx->idx]);
    cx->values[cx->idx] = tmp;
    cx->idx = (cx->idx + 1) % cx->max;
    return cx->sum / cx->max;
}

static average_cx_t *average_alloc_cx(int max)
{
    int len;
    average_cx_t *cx;

    len = offsetof(average_cx_t, values[max]);
    cx = calloc(1,len);
    cx->max = max;
    return cx;
}
