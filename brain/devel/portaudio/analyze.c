// xxx
// - comments
// - support microphone option
// - adjustment knobs for tuning
// - integrate with leds 
// - test on rpi
// - add Time option, and filter params options, so different filter params can be scripted  MAYBE

// xxx later
// - make a stanalone demo pgm and put in new repo  LATER

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <math.h>

#include <sf_utils.h>
#include <audio_filters.h>
#include <poly_fit.h>

//
// defines
//

#define SAMPLE_RATE  48000
#define MAX_CHAN     4

#define DEBUG_FRAME_RATE    1
#define DEBUG_ANALYZE_SOUND 2

//
// typedefs
//

//
// variables
//

static int get_data_from_file_init_status;
static int debug = DEBUG_ANALYZE_SOUND;

//
// prototpes
//

static void *get_data_from_file_thread(void *cx);
static void process_data(float *frame, double time_secs, void *cx);

static void dbgpr(char *fmt, ...) __attribute__ ((format (printf, 1, 2)));
static void *dbgpr_thread(void *cx);

static void sleep_until(uint64_t time_next_us);
static double squared(double v);
static double normalize_angle(double angle);
static char *stars(double v, double max_v, int max_stars, char *s);
static uint64_t microsec_timer(void);

// -----------------  MAIN  ------------------------------------------------

int main(int argc, char **argv)
{
    char *file_name = "4mic_0.wav";  // xxx temporary default
    pthread_t tid;

    #define USAGE \
    "usage: analyze [-f file_name.wav]"

    // use line buffered stdout
    setlinebuf(stdout);

    // get options
    // xxx add mic option, and default to mic
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

    // create dbgpr_thread, this thread supports process_data calls to dbgpr;
    // needed because process_data should avoid calls that may take a long time to complete
    pthread_create(&tid, NULL, dbgpr_thread, NULL);

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
    while (true) {
        pause();
    }

    // done
    return 0;
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
    printf("read okay, file_name=%s  max_frames=%d\n", file_name, max_frames);

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

// -----------------  PROCESS 4 CHANNEL AUDIO DATA --------------------------

#if 0
#define CCA_MICX     0  // cross
#define CCA_MICY     2
#define CCB_MICX     1
#define CCB_MICY     3
#define ANGLE_OFFSET 45
#else
#define CCA_MICX     0  // adjacanet
#define CCA_MICY     1
#define CCB_MICX     0
#define CCB_MICY     3
#define ANGLE_OFFSET 0
#endif

#define MIN_START_AMP    0.5
#define MIN_END_AMP      5.0 
#define MAX_CHAN_DATA    (SAMPLE_RATE/2)
#define WINDOW_DURATION  ((double)MAX_CHAN_DATA / SAMPLE_RATE)
#define                  N 15

static void process_data(float *frame, double time_now, void *cx)
{
    // xxx make this an inline and check the offset
    #define DATA(_chan,_offset) \
        data [ _chan ] [ idx+(_offset) >= 0 ? idx+(_offset) : idx+(_offset)+MAX_CHAN_DATA ]

    static float data[MAX_CHAN][MAX_CHAN_DATA];
    static int   idx;

    // notes:
    // - time values used in this routine have units of seconds

    // print the frame rate once per sec
    if (debug & DEBUG_FRAME_RATE) {
        static uint64_t frame_count;
        static double   time_last_frame_rate_print;
        static uint64_t frame_count_last_print;
        frame_count++;
        if (time_now - time_last_frame_rate_print >= 1) {
            dbgpr("T=%0.3f: FRAME RATE = %d\n", time_now, (int)(frame_count-frame_count_last_print));
            frame_count_last_print = frame_count;
            time_last_frame_rate_print = time_now;
        }
    }

    // increment data idx
    idx = (idx + 1) % MAX_CHAN_DATA;

    // copy the input frame data to the static 'data' arrray
    // xxx comment about filtering
    for (int chan = 0; chan < MAX_CHAN; chan++) {
        static double filter_cx[MAX_CHAN];
        DATA(chan,0) = high_pass_filter(frame[chan], &filter_cx[chan], 0.75);
    }

    // corr[10] is center
    static double cca[2*N+1];  
    static double ccb[2*N+1];
    for (int i = -N; i <= N; i++) {
        cca[i+N] += DATA(CCA_MICX,-N) * DATA(CCA_MICY,-(N+i))  -
                     DATA(CCA_MICX,-((MAX_CHAN_DATA-1)-N)) * DATA(CCA_MICY,-((MAX_CHAN_DATA-1)-N+i));
        ccb[i+N] += DATA(CCB_MICX,-N) * DATA(CCB_MICY,-(N+i))  -
                     DATA(CCB_MICX,-((MAX_CHAN_DATA-1)-N)) * DATA(CCB_MICY,-((MAX_CHAN_DATA-1)-N+i));
    }

    // determine the avg amplitude ovr the past WINDOW_DURATION for chan 0
    static double  amp;
    amp += (squared(DATA(0,0)) - squared(DATA(0,-(MAX_CHAN_DATA-1))));
    // dbgpr("T=%0.3f: amp=%10.3f\n", time_now, amp);

    // xxx comments
    static double time_sound_start;
    bool got_sound = false;
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
            // xxx seeing this printed now for 4mic_270.wav
            dbgpr("T=%0.3f: DISCARD SOUND amp=%0.3f  intvl=%0.3f ... %0.3f\n", 
                   time_now, amp, time_sound_start, time_now);
        }
        time_sound_start = 0;
    }

    // if there is not sound data to analyze then return
    if (got_sound == false) {
        return;
    }

    // ----------------------------
    // xxx comment
    // ----------------------------

    int           max_cca_idx=-99, max_ccb_idx=-99;
    double        max_cca=0, max_ccb=0, coeffs[3], cca_x, ccb_x, angle;
    static double x[2*N+1];
    if (x[0] == 0) { // init on first call
        for (int i = -N; i <= N; i++) x[i+N] = i;
    }

    // xxx
    for (int i = -N; i <= N; i++) {
        if (cca[i+N] > max_cca) {
            max_cca = cca[i+N];
            max_cca_idx = i;
        }
        if (ccb[i+N] > max_ccb) {
            max_ccb = ccb[i+N];
            max_ccb_idx = i;
        }
    }

    // xxx if xxx explain
    if ((max_cca_idx <= -N || max_cca_idx >= N) ||
        (max_ccb_idx <= -N || max_ccb_idx >= N))
    {
        dbgpr("T=%0.3f: ANALYZE SOUND - ERROR max_cca_idx=%d max_ccb_idx=%d\n", 
              time_now, max_cca_idx, max_ccb_idx);
        return;
    }

    // fit to parabola (degree 2 polynomial)
    poly_fit(3, &x[max_cca_idx-1+N], &cca[max_cca_idx-1+N], 2, coeffs);
    cca_x = -coeffs[1] / (2 * coeffs[2]);
    poly_fit(3, &x[max_ccb_idx-1+N], &ccb[max_ccb_idx-1+N], 2, coeffs);
    ccb_x = -coeffs[1] / (2 * coeffs[2]);

    // determine the angle to the sound source
    angle = atan2(ccb_x, cca_x) * (180/M_PI);
    angle = normalize_angle(angle + ANGLE_OFFSET);

    // prints
    if (debug & DEBUG_ANALYZE_SOUND) {
        dbgpr("T=%0.3f: ANALYZE SOUND - amp=%0.3f  intvl=%0.3f ... %0.3f\n", 
                       time_now, amp, time_now-WINDOW_DURATION, time_now);
        for (int i = -N; i <= N; i++) {
            char s1[100], s2[100];
            dbgpr("%3d: %5.1f %-30s - %5.1f %-30s\n",
                   i,
                   cca[i+N], stars(cca[i+N], max_cca, 30, s1),
                   ccb[i+N],  stars(ccb[i+N], max_ccb, 30, s2));
        }
        dbgpr("       LARGEST AT %-10.3f                 LARGEST AT %-10.3f\n", cca_x, ccb_x);
        dbgpr("       SOUND ANGLE = %0.1f degs *****\n", angle);
        dbgpr("\n");
    }
}

// -----------------  PROCESS DATA - DEBUG PRINT SUPPORT  ------------------

#define MAX_DBGPR 1000

static char dbgpr_buff[MAX_DBGPR][150];
static volatile uint64_t prints_produced;
static volatile uint64_t prints_consumed;

static void dbgpr(char *fmt, ...)
{
    int idx, cnt;
    va_list ap;

    // if out of print buffers then return
    if (prints_produced - prints_consumed >= MAX_DBGPR) {
        return;
    }

    // print to buffer
    va_start(ap, fmt);
    idx = (prints_produced % MAX_DBGPR);
    cnt = vsnprintf(dbgpr_buff[idx], 150, fmt, ap);
    if (cnt >= 150) {
        dbgpr_buff[idx][150-2] = '\n';
    }
    va_end(ap);
    __sync_synchronize();

    // increment print count
    prints_produced++;
}

static void *dbgpr_thread(void *cx)
{
    int idx;

    while (true) {
        while (prints_produced == prints_consumed) {
            usleep(10000);
        }

        while (prints_produced > prints_consumed) {
            idx = (prints_consumed % MAX_DBGPR);
            printf("%s", dbgpr_buff[idx]);
            prints_consumed++;
        }
    }

    return NULL;
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

static double normalize_angle(double angle)
{
    if (angle < 0) {
        while (angle < 0) angle += 360;
    } else if (angle > 360) {
        while (angle > 360) angle -= 360;
    }
    return angle;
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

static uint64_t microsec_timer(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC,&ts);
    return  ((uint64_t)ts.tv_sec * 1000000) + ((uint64_t)ts.tv_nsec / 1000);
}

