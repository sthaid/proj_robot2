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
#include <math.h>

#include <util_misc.h>  // xxx needed?
#include <sf_utils.h>
#include <filter_utils.h>

//
// defines
//

#define SAMPLE_RATE  48000
#define MAX_CHAN     4

//
// typedefs
//

//
// variables
//

static int   get_data_from_file_init_status;

//
// prototpes
//

static void sleep_until(uint64_t time_next_us);
static double squared(double v);
static void *get_data_from_file_thread(void *cx);
static void process_data(float *frame, double time_secs, void *cx);

static char *stars(double v, double max_v, int max_stars, char *s);

// -----------------  MAIN  ------------------------------------------------

int main(int argc, char **argv)
{
    pthread_t tid;
    //char *file_name = "4mic_0.wav";  // xxx arg for this and mic
    char *file_name = "4mic_270.wav";  // xxx arg for this and mic

    setlinebuf(stdout);

    printf("STARTING ...\n");

    // create get_data_from_file_thread, and
    // wait for it to complete initialization
    if (file_name) {
        get_data_from_file_init_status = 1;
        pthread_create(&tid, NULL, get_data_from_file_thread, file_name);
        while (get_data_from_file_init_status == 1) {
            usleep(10000);
        }
        if (get_data_from_file_init_status != 0) {
            printf("FATAL: get_data_from_file init failed\n");
            return 1;
        }
    }

    // pause forever
    printf("\nRUNNING ...\n");
    while (true) {
        pause();
    }

    // done
    return 0;
}

// -----------------  UTILS  -----------------------------------------------

static void sleep_until(uint64_t time_next_us)
{
    uint64_t time_now_us = microsec_timer();

    if (time_next_us > time_now_us) {
        usleep(time_next_us - time_now_us);
    }
}

static double squared(double v)
{
    return v * v;
}

// -----------------  GET DATA FROM FILE THREAD  ---------------------------

static void *get_data_from_file_thread(void *cx)
{
    char    *file_name = cx;
    float   *data;
    int      max_data, max_chan, max_frames, sample_rate, frame_idx=0, i;
    uint64_t time_start_us, time_next_us;
    double   time_secs;

    // read file
    if (strstr(file_name, ".wav") == NULL) {
        printf("ERROR: file_name must have '.wav' extension\n");
        get_data_from_file_init_status = -1;
        return NULL;
    }
    if (sf_read_wav_file(file_name, &data, &max_chan, &max_data, &sample_rate) < 0) {
        printf("ERROR: sf_read_wav_file %s failed\n", optarg);
        get_data_from_file_init_status = -1;
        return NULL;
    }
    if (sample_rate != SAMPLE_RATE) {
        printf("ERROR: file sample_rate=%d, must be %d\n", sample_rate, SAMPLE_RATE);
        get_data_from_file_init_status = -1;
        return NULL;
    }
    if (max_chan != MAX_CHAN) {
        printf("ERROR: file max_chan=%d, must be %d\n", max_chan, MAX_CHAN);
        get_data_from_file_init_status = -1;
        return NULL;
    }
    max_frames = max_data / max_chan;
    printf("XXX FILE MAX_FRAMES %d\n", max_frames);

    // set get_data_from_file_init_status to success
    get_data_from_file_init_status = 0;

    // loop, passing file data to routine for procesing
    time_start_us = time_next_us = microsec_timer();
    while (true) {
        // provide 48 samples to the process_data routine
        time_secs = (microsec_timer()-time_start_us) / 1000000.;
        for (i = 0; i < 48; i++) {
            process_data(&data[frame_idx*max_chan], time_secs, NULL);
            frame_idx = (frame_idx + 1) % max_frames;
        }

        // sleep until time_next_us
        time_next_us += (48 * 1000000) / SAMPLE_RATE;
        sleep_until(time_next_us);
    }

    return NULL;
}

// -------------------------------------------------------------------------

static void process_data(float *frame, double time_now, void *cx)
{
#if 1
    // window = 0.5 secs
    #define MIN_START_AMP    1.0   
    #define MIN_END_AMP      10.0
    #define MAX_CHAN_DATA    (SAMPLE_RATE/2)
#endif
#if 0
    // window = 0.1 secs
    #define MIN_START_AMP    0.4
    #define MIN_END_AMP      4.0
    #define MAX_CHAN_DATA    (SAMPLE_RATE/10)
#endif

    #define WINDOW_DURATION   ((double)MAX_CHAN_DATA / SAMPLE_RATE)

    // xxx make this an inline and check the offset
    #define DATA(_chan,_offset) \
        data [ _chan ] [ idx+(_offset) >= 0 ? idx+(_offset) : idx+(_offset)+MAX_CHAN_DATA ]

    static float    data[MAX_CHAN][MAX_CHAN_DATA];
    static int      idx;
    static double   amp;
    static double   time_sound_start;

    bool got_sound;
    int  i;

    // notes:
    // - time values used in this routine have units of seconds

#if 0
    // print the frame rate once per sec
    static uint64_t frame_count;
    static double   time_last_frame_rate_print;
    static uint64_t frame_count_last_print;
    frame_count++;
    if (time_now - time_last_frame_rate_print >= 1) {
        printf("FRAME RATE = %d\n", (int)(frame_count-frame_count_last_print));
        frame_count_last_print = frame_count;
        time_last_frame_rate_print = time_now;
    }
#endif

    // increment data idx
    idx = (idx + 1) % MAX_CHAN_DATA;

    // xxx filter
    static double filter_cx[MAX_CHAN][10];
#if 0
    for (int chan = 0; chan < MAX_CHAN; chan++) {
        frame[chan] = low_pass_filter_ex(frame[chan], filter_cx[chan], 1, 0.80);
    }
#endif

    // copy the input frame data to the static 'data' arrray
    for (int chan = 0; chan < MAX_CHAN; chan++) {
        //DATA(chan,0) = frame[chan];
        //DATA(chan,0) = low_pass_filter_ex(frame[chan], filter_cx[chan], 1, 0.95);
        //DATA(chan,0) = high_pass_filter_ex(frame[chan], filter_cx[chan], 1, 0.95);
        //DATA(chan,0) = high_pass_filter_ex(frame[chan], filter_cx[chan], 1, 0.80);
        DATA(chan,0) = high_pass_filter_ex(frame[chan], filter_cx[chan], 1, 0.75);  // looks okay
    }


#if 0
    // XXX temp
    if (time_now > 1 && time_now < 3) {
        printf("%0.3f  -  %10.3f\n", time_now, amp);
    }
#endif

    // corr[10] is center
    static double corr02[41];
    static double corr13[41];
    const int max = MAX_CHAN_DATA-1;
#if 0
    for (i = -20; i <= 20; i++) {
        corr02[i+20] += DATA(0,-20) * DATA(2,-(20+i))  -
                        DATA(0,-(max-20)) * DATA(2,-(max-20+i));
    }
    for (i = -20; i <= 20; i++) {
        corr13[i+20] += DATA(1,-20) * DATA(3,-(20+i))  -
                        DATA(1,-(max-20)) * DATA(3,-(max-20+i));
    }
#endif
    for (i = -20; i <= 20; i++) {
        corr02[i+20] += DATA(0,-20) * DATA(1,-(20+i))  -
                        DATA(0,-(max-20)) * DATA(1,-(max-20+i));
    }
    for (i = -20; i <= 20; i++) {
        corr13[i+20] += DATA(0,-20) * DATA(3,-(20+i))  -
                        DATA(0,-(max-20)) * DATA(3,-(max-20+i));
    }

    // determine the avg amplitude ovr the past WINDOW_DURATION for chan 0
    amp += (squared(DATA(0,0)) - squared(DATA(0,-(MAX_CHAN_DATA-1))));

    // xxx comments
    got_sound = false;
    if (amp > MIN_START_AMP) {
        if (time_sound_start == 0) {
            time_sound_start = time_now;
        }
    } else {
        time_sound_start = 0;
    }
    if (time_sound_start > 0 && time_now >= time_sound_start + WINDOW_DURATION) {
        got_sound = (amp > MIN_END_AMP);
        if (!got_sound) {
            printf("%0.3f - *** DISCARD *** SOUND amp=%0.3f  intvl=%0.3f ... %0.3f\n", 
                   time_now, amp, time_sound_start, time_now);
        }
        time_sound_start = 0;
    }

#if 0
    static int flag = 1;
    if (flag) {
        if (time_now >= 1) {
            printf("TEST\n");
            got_sound = true;
            flag = false;
        }
    }
#endif

    // if there is not sound data to analyze then return
    if (got_sound == false) {
        return;
    }

    // xxx comment
    printf("%0.3f - ANALYZE SOUND amp=%0.3f  intvl=%0.3f ... %0.3f\n", 
                   time_now, amp, time_sound_start, time_now);

    double max_corr02=0, max_corr13=0;
    for (i = -20; i <= 20; i++) {
        if (corr02[i+20] > max_corr02) max_corr02 = corr02[i+20];
        if (corr13[i+20] > max_corr13) max_corr13 = corr13[i+20];
    }

    for (i = -20; i <= 20; i++) {
        char s1[100], s2[100];
        printf("%3d: %5.1f %-30s - %5.1f %-30s\n",
               i,
               corr02[i+20], stars(corr02[i+20], max_corr02, 30, s1),
               corr13[i+20],  stars(corr13[i+20], max_corr13, 30, s2));
    }


    //memset(corr02,0,sizeof(corr02));
    //memset(corr13,0,sizeof(corr13));
    //memset(filter_cx,0,sizeof(filter_cx));
    // xxx continue here
}

static char *stars(double v, double max_v, int max_stars, char *s)
{
    if (v < 0) v = 0;
    if (v > max_v) v = max_v;

    int n = nearbyint(v / max_v * max_stars);
    memset(s, '*', n);
    s[n] = '\0';

    return s;
}
