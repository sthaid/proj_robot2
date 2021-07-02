// xxx
// - comments
// - support microphone option
// - adjustment knobs for tuning
// - integrate with leds 
// - test on rpi
// - add Time option, and filter params options, so different filter params can be scripted  MAYBE
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

#define MAX_FRAME        (MS_TO_FRAMES(500))
#define MIN_AMP          0.0003
#define MIN_INTEGRAL     (MIN_AMP * 2 * MS_TO_FRAMES(150))

#define WINDOW_DURATION  ((double)MAX_FRAME / SAMPLE_RATE)
#define MS_TO_FRAMES(ms) (SAMPLE_RATE * (ms) / 1000)

#define                  N 15   // N is half the number of cross correlations 

// xxx check all printts, incl FC
static void process_data(float *frame, double time_secs, void *cx)
{
    // xxx make this an inline and check the offset
    #define DATA(_chan,_offset) \
        data [ _chan ] [ idx+(_offset) >= 0 ? idx+(_offset) : idx+(_offset)+MAX_FRAME ]

    static float    data[MAX_CHAN][MAX_FRAME];
    static int      idx;  // xxx call this data_idx
    static uint64_t frame_cnt;

    // increment data idx, and frame_cnt
    idx = (idx + 1) % MAX_FRAME;
    frame_cnt++;

    // print the frame rate once per sec
    if (debug & DEBUG_FRAME_RATE) {
        static double   time_last_frame_rate_print;
        static uint64_t frame_cnt_last_print;
        if (time_secs - time_last_frame_rate_print >= 1) {
            dbgpr("T=%0.3f: FRAME RATE = %d\n", time_secs, (int)(frame_cnt-frame_cnt_last_print));
            frame_cnt_last_print = frame_cnt;
            time_last_frame_rate_print = time_secs;
        }
    }

    // apply high pass filter to the input frame and store in 'data' array
    for (int chan = 0; chan < MAX_CHAN; chan++) {
        static double filter_cx[MAX_CHAN];
        DATA(chan,0) = high_pass_filter(frame[chan], &filter_cx[chan], 0.75);
    }

    // compute cross correlations for the 2 pairs of mic channels
    static double cca[2*N+1];  
    static double ccb[2*N+1];
    for (int i = -N; i <= N; i++) {
        cca[i+N] += DATA(CCA_MICX,-N) * DATA(CCA_MICY,-(N+i))  -
                    DATA(CCA_MICX,-((MAX_FRAME-1)-N)) * DATA(CCA_MICY,-((MAX_FRAME-1)-N+i));
        ccb[i+N] += DATA(CCB_MICX,-N) * DATA(CCB_MICY,-(N+i))  -
                    DATA(CCB_MICX,-((MAX_FRAME-1)-N)) * DATA(CCB_MICY,-((MAX_FRAME-1)-N+i));
    }

    // compute average amp over the past 20 ms, this is compted using mic chan 0
    static double amp_sum;
    double amp;
    amp_sum += (squared(DATA(0,0)) - squared(DATA(0,-MS_TO_FRAMES(20))));
    amp = amp_sum / MS_TO_FRAMES(20);
    //dbgpr("FC=%ld  T=%0.3f: sum=%0.6f  amp=%10.6f\n", frame_cnt, time_secs, sum, amp);

    // determine if sound data should now be analyzed;
    // if so, then the 'analyze' flag is set;
    // summary:
    // - if amp is > MIN_AMP then start_frame_cnt is set, indicating that 
    //   a block of frames is being considered to be analyzed
    // - if after 150 ms after start_frame_cnt was set, there was not much
    //   total amplitude over the past 150 ms; then cancel considering this data 
    // - if not cancelled, then the data will be analyzed once the frame_cnt advances
    //   to 480 ms beyond the start_frame_cnt
    // - the analysis that is performed covers a 500 ms range; so the range that will
    //   be analyzed extends from 20 ms before the start_frame_cnt to now
    bool            analyze = false;
    static uint64_t start_frame_cnt;
    static double   integral, trigger_integral;

    if (start_frame_cnt == 0) {
        if (amp > MIN_AMP) {
            start_frame_cnt = frame_cnt;
            integral = 0;
            trigger_integral = 0;
            dbgpr("START FRAME CNT = %ld\n", start_frame_cnt);
        }
    } else {
        integral += squared(DATA(0,0));
        //dbgpr("integral = %0.6f\n", integral);
        if (frame_cnt == start_frame_cnt + MS_TO_FRAMES(150)) {
            dbgpr("CHECKING FOR MIN_INTEGRAL %0.6f  %0.6f\n", integral, MIN_INTEGRAL);
            trigger_integral = integral;
            if (integral < MIN_INTEGRAL) {
                dbgpr("*** FAILED\n");
                start_frame_cnt = 0;
            }
        } else if (frame_cnt == start_frame_cnt + MS_TO_FRAMES(480)) {
            dbgpr("START ANALYZE\n");
            analyze = true;
            start_frame_cnt = 0;
        }
    }

    // if there is not sound data to analyze then return
    if (analyze == false) {
        return;
    }

    // -----------------------------------------------------
    // the following code determines the angle to the sound
    //                         0
    //                         ^
    //              270 <- RESPEAKER -> 90
    //                         v
    //                        180
    // -----------------------------------------------------

    int           max_cca_idx=-99, max_ccb_idx=-99;
    double        max_cca=0, max_ccb=0, coeffs[3], cca_x, ccb_x, angle;
    static double x[2*N+1];
    if (x[0] == 0) { // init on first call
        for (int i = -N; i <= N; i++) x[i+N] = i;
    }

    // Each of the 2 pairs of mics has 2*N+1 cross correlation results that
    // is computed by the code near the top of this routine.
    // Determine the max cross-corr value for the sets of cross correlations
    // performed for the 2 pairs of mics.
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

    // The deviation of the max cross correlation from center should be 
    // limitted by the speed of sound, the distance between the mics, and
    // the sample_rate. Using:
    // - distance between mics = 0.061 m
    // - speed of sond         = 343 m/s
    // - sample rate           = 48000 samples per second
    // time = .061 / 343 = .00018
    // samples = .00018 * 48000 = 8.5
    //
    // When using N=15 there is room for location of the max to be in the
    // range of -15 to +15 samples.
    // 
    // This code block ensures that the max is in the range -14 to +14;
    // this range is needed (insead of -15 to +15) because the poly_fit 
    // that is performed below is using the max value and 1 value on either
    // side of the max.
    if ((max_cca_idx <= -N || max_cca_idx >= N) ||
        (max_ccb_idx <= -N || max_ccb_idx >= N))
    {
        dbgpr("T=%0.3f: ANALYZE SOUND - ERROR max_cca_idx=%d max_ccb_idx=%d\n", 
              time_secs, max_cca_idx, max_ccb_idx);
        return;
    }

    // Instead of using the max_cca/b_idx that is obtained above; this code
    // attempts to find a better value by fitting a 2nd degree polynomial 
    // (a parabola) using the max value, and the 2 values on either side of the max.
    //
    // For example, the max value is located at 7 samples; but the following
    // graph indicates that the 'true' max would be a little larger than 7.
    //   6:   5.3 **************************
    //   7:   6.2 ******************************
    //   8:   5.5 ***************************  
    // The vale computed, for this example, is: 7.064
    poly_fit(3, &x[max_cca_idx-1+N], &cca[max_cca_idx-1+N], 2, coeffs);
    cca_x = -coeffs[1] / (2 * coeffs[2]);
    poly_fit(3, &x[max_ccb_idx-1+N], &ccb[max_ccb_idx-1+N], 2, coeffs);
    ccb_x = -coeffs[1] / (2 * coeffs[2]);

    // determine the angle to the sound source
    angle = atan2(ccb_x, cca_x) * (180/M_PI);
    angle = normalize_angle(angle + ANGLE_OFFSET);

    // debug prints
    if (debug & DEBUG_ANALYZE_SOUND) {
        dbgpr("T=%0.3f: ANALYZE SOUND - trigger_integral=%0.3f %0.3f  integral=%0.3f  intvl=%0.3f ... %0.3f\n", 
                       time_secs, trigger_integral, MIN_INTEGRAL, integral, time_secs-WINDOW_DURATION, time_secs);
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

// The process_data routine should not call printf directly, because that could
// cause unpredicatable execution delays. Instead, process_data calls dbgpr(),
// which prints to dbgpr_buff. And, the dbgpr_thread performes the printf.

#define MAX_DBGPR 10000

static char dbgpr_buff[MAX_DBGPR][150];
static volatile uint64_t prints_produced;
static volatile uint64_t prints_consumed;

static void dbgpr(char *fmt, ...)
{
    int idx, cnt;
    va_list ap;

    // if out of print buffers then return
    if (prints_produced - prints_consumed == MAX_DBGPR) {
        static bool printed;
        if (printed == false) {
            printf("WARNING: dbgpr is delaying\n");
            printed = true;
        }
        while (prints_produced - prints_consumed == MAX_DBGPR) {
            usleep(100);
        }
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
            __sync_synchronize();

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

