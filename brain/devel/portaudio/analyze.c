// xxx
// - make a stanalone demo pgm and put in new repo  LATER
// - support microphone option
// - define for using cross microphone config
// - use parabola fit to find the peek
//   - and use atan to det direction
// - adjustment knobs for tuning
// - integrate with leds 
// - test on rpi
// - add Time option, and filter params options, so different filter params can be scripted

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
#include <filter_utils.h>  // xxx rename to filters.h
#include <poly_fit.h>

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

static double normalize(double angle);
static char *stars(double v, double max_v, int max_stars, char *s);
static void print_frame_rate(double time_now);

// -----------------  MAIN  ------------------------------------------------

int main(int argc, char **argv)
{
    char *file_name = "4mic_0.wav";  // xxx temp default

    #define USAGE \
    "usage: analyze [-f file_name.wav]"

    // use line buffered stdout
    setlinebuf(stdout);

    // get options
    // xxx add mic, and default to mic
    while (true) {
        int ch = getopt(argc, argv, "f:h");
        if (ch == -1) {
            break;
        }
        switch (ch) {
        case 'f':
            file_name = optarg;
            if (strstr(file_name, ".wav") == NULL) {
                printf("ERROR: file_name must have '.wav' extension\n");
                return 1;
            }
            break;
        case 'h':
            printf("%s\n", USAGE);
            return 0;
            break;
        default:
            return 1;
        };
    }

    // print startup msg
    printf("file_name = %s\n", file_name);
    // xxx print other stuff, like filter options

    // create get_data_from_file_thread, and
    // wait for it to complete initialization
    if (file_name) {
        pthread_t tid;

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
    while (true) {
        // xxx put debug prints in here
        // xxx maybe want debug controls too
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

// time values used in this routine have units of seconds
static void process_data(float *frame, double time_now, void *cx)
{
    // window = 0.5 secs
    #define MIN_START_AMP    0.5
    #define MIN_END_AMP      5.0 
    #define MAX_CHAN_DATA    (SAMPLE_RATE/2)

    #define WINDOW_DURATION   ((double)MAX_CHAN_DATA / SAMPLE_RATE)

    // xxx make this an inline and check the offset
    #define DATA(_chan,_offset) \
        data [ _chan ] [ idx+(_offset) >= 0 ? idx+(_offset) : idx+(_offset)+MAX_CHAN_DATA ]

    static float    data[MAX_CHAN][MAX_CHAN_DATA];
    static int      idx;
    static double   amp;
    static double   time_sound_start;
    static double   filter_cx[MAX_CHAN][10];  // xxx move decls to where first used

    bool got_sound;
    int  i;

    // print the frame rate once per sec
    print_frame_rate(time_now);

    // increment data idx
    idx = (idx + 1) % MAX_CHAN_DATA;

    // copy the input frame data to the static 'data' arrray
    // xxx comment about filtering
    // xxx dont need the ex filter
    for (int chan = 0; chan < MAX_CHAN; chan++) {
        DATA(chan,0) = high_pass_filter_ex(frame[chan], filter_cx[chan], 1, 0.75);
    }

#if 0 
    // xxx used to unit test amplitude monitoring
    if (time_now > 1 && time_now < 3) {
        printf("%0.3f  -  %10.3f\n", time_now, amp);
    }
#endif

//cross_corr_a
//corr_a
//corr_b
//cca
//ccb
    // corr[10] is center
    static double cca[41];
    static double ccb[41];
    const int max = MAX_CHAN_DATA-1;
#if 1  // xxx use defines for which channels
    for (i = -20; i <= 20; i++) {
        cca[i+20] += DATA(0,-20) * DATA(2,-(20+i))  -
                        DATA(0,-(max-20)) * DATA(2,-(max-20+i));
    }
    for (i = -20; i <= 20; i++) {
        ccb[i+20] += DATA(1,-20) * DATA(3,-(20+i))  -
                        DATA(1,-(max-20)) * DATA(3,-(max-20+i));
    }
#else
    for (i = -20; i <= 20; i++) {
        cca[i+20] += DATA(0,-20) * DATA(1,-(20+i))  -
                        DATA(0,-(max-20)) * DATA(1,-(max-20+i));
    }
    for (i = -20; i <= 20; i++) {
        ccb[i+20] += DATA(0,-20) * DATA(3,-(20+i))  -
                        DATA(0,-(max-20)) * DATA(3,-(max-20+i));
    }
#endif

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
        if (got_sound == false) {
            // xxx see this printed now for 4mic_270.wav
            printf("%0.3f - *** DISCARD *** SOUND amp=%0.3f  intvl=%0.3f ... %0.3f\n", 
                   time_now, amp, time_sound_start, time_now);
        }
        time_sound_start = 0;
    }

    // if there is not sound data to analyze then return
    if (got_sound == false) {
        return;
    }

    // xxx comment
    printf("%0.3f - ANALYZE SOUND amp=%0.3f  intvl=%0.3f ... %0.3f\n", 
                   time_now, amp, time_now-WINDOW_DURATION, time_now);

    double max_cca=0, max_ccb=0;
    int max_cca_idx=-1, max_ccb_idx=-1;
    for (i = -20; i <= 20; i++) {
        if (cca[i+20] > max_cca) {
            max_cca = cca[i+20];
            max_cca_idx = i;
        }
        if (ccb[i+20] > max_ccb) {
            max_ccb = ccb[i+20];
            max_ccb_idx = i;
        }
    }

    for (i = -20; i <= 20; i++) {
        char s1[100], s2[100];
        printf("%3d: %5.1f %-30s - %5.1f %-30s\n",
               i,
               cca[i+20], stars(cca[i+20], max_cca, 30, s1),
               ccb[i+20],  stars(ccb[i+20], max_ccb, 30, s2));
    }

    // if the max_corr idx is invalid then return
    // 


    // fit to parabola (degree 2 polynomial)
    double coeffs[3] = {0,0,0};
    double x[41];
    for (i = -20; i <= 20; i++) {
        x[i+20] = i;
    }

    printf("max_cor02_idx, val = %d %0.3f\n", max_cca_idx, max_cca);

    //poly_fit(5, &x[max_cca_idx-2+20], &cca[max_cca_idx-2+20], 2, coeffs);
    poly_fit(3, &x[max_cca_idx-1+20], &cca[max_cca_idx-1+20], 2, coeffs);
    //printf("COEFFS %0.3f %0.3f %0.3f\n", coeffs[0], coeffs[1], coeffs[2]);
    double answer02 = -coeffs[1] / (2 * coeffs[2]);
    printf("ANSWER02 %0.3f\n", answer02);

    poly_fit(3, &x[max_ccb_idx-1+20], &ccb[max_ccb_idx-1+20], 2, coeffs);
    //printf("COEFFS %0.3f %0.3f %0.3f\n", coeffs[0], coeffs[1], coeffs[2]);
    double answer13 = -coeffs[1] / (2 * coeffs[2]);
    printf("ANSWER13 %0.3f\n", answer13);

    double angle;
    //angle = atan2(answer02, answer13) * (180/M_PI);
    //angle = normalize(angle-90);
    angle = atan2(answer13, answer02) * (180/M_PI);
#if 0
    angle = normalize(angle);
#else
    angle = normalize(angle+45);
#endif
    printf("***** angle = %0.1f degs *****\n", angle);

// plot y = -16.136 * x^2 + 6.4 * x - 0.4

}

static double normalize(double angle)
{
    if (angle < 0) {
        while (angle < 0) angle += 360;
    } else if (angle > 360) {
        while (angle > 360) angle -= 360;
    }
    return angle;
}


// xxx move to debug section
static char *stars(double v, double max_v, int max_stars, char *s)
{
    if (v < 0) v = 0;
    if (v > max_v) v = max_v;

    int n = nearbyint(v / max_v * max_stars);
    memset(s, '*', n);
    s[n] = '\0';

    return s;
}

static void print_frame_rate(double time_now)
{
    static uint64_t frame_count;
    static double   time_last_frame_rate_print;
    static uint64_t frame_count_last_print;

    // only needed for unit test
    return;

    frame_count++;
    if (time_now - time_last_frame_rate_print >= 1) {
        printf("FRAME RATE = %d\n", (int)(frame_count-frame_count_last_print));
        frame_count_last_print = frame_count;
        time_last_frame_rate_print = time_now;
    }
}
