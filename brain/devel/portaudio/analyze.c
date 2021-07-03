// xxx
// - comments
// - adjustment knobs for tuning
// - integrate with leds 
// - test on rpi
// - add Time option, and filter params options, so different filter params can be scripted  MAYBE
// - make a stanalone demo pgm and put in new repo  LATER
//
// - identify a set of words to test with
// - see how various adjustments, such as filter helps
// - try to rule out graphs that don't have a clear single peak
// - increase sound sensitivity

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>
#include <inttypes.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <math.h>

#include <wiringPi.h>

#include <pa_utils.h>
#include <sf_utils.h>
#include <audio_filters.h>
#include <poly_fit.h>
#include <apa102.h>

//
// defines
//

#define SAMPLE_RATE  48000
#define MAX_CHAN     4

#define DEBUG_FRAME_RATE    1
#define DEBUG_ANALYZE_SOUND 2
#define DEBUG_AMP           4
#define DEBUG_AMP_VERBOSE   8

#define DATA_SRC_MIC  1
#define DATA_SRC_FILE 2

//
// typedefs
//

typedef struct {
    double   angle;
    uint64_t angle_frame_cnt;
} leds_t;

//
// variables
//

static int       debug = DEBUG_ANALYZE_SOUND;

static bool      prog_terminating;

static pthread_t tid_dbgpr_thread;
static pthread_t tid_leds_thread;
static pthread_t tid_get_data_from_mic_thread;
static pthread_t tid_get_data_from_file_thread;

static uint64_t  frame_cnt;
static leds_t    leds;

//
// prototpes
//

static int init_get_data_from_mic(char *dev_name);
static void * get_data_from_mic_thread(void *cx);
static int init_get_data_from_file(char *file_name);
static void *get_data_from_file_thread(void *cx);

static int process_data(const float *frame, void *cx);

static void dbgpr(char *fmt, ...) __attribute__ ((format (printf, 1, 2)));
static void *dbgpr_thread(void *cx);

static void sleep_until(uint64_t time_next_us);
static double squared(double v);
static double normalize_angle(double angle);
static char *stars(double v, double max_v, int max_stars, char *s);
static uint64_t microsec_timer(void);

static int leds_init(void);
static void * leds_thread(void * cx);
static void convert_angle_to_led_num(double angle, int *led_a, int *led_b);

// -----------------  MAIN  ------------------------------------------------

int main(int argc, char **argv)
{
    int       data_src      = DATA_SRC_MIC;
    char     *data_src_name = "seeed-4mic-voicecard";

    // use line buffered stdout
    setlinebuf(stdout);

   #define USAGE \
    "usage: analyze [-d indev] [-f filename]\n" \
    "       - use either -d or -f\n" \
    "       - default is -d seeed-4mic-voicecard"

    // get options
    while (true) {
        int ch = getopt(argc, argv, "f:d:h");
        if (ch == -1) {
            break;
        }
        switch (ch) {
        case 'f':
            data_src_name = optarg;
            data_src = DATA_SRC_FILE;
            break;
        case 'd':
            data_src_name = optarg;
            data_src = DATA_SRC_MIC;
            break;
        case 'h':
            printf("%s\n", USAGE);
            return 0;
            break;
        default:
            return 1;
        };
    }

    // init leds, which will be used to indicate the direction of the sound
    if (leds_init() < 0) {
        printf("ERROR: leds_int failed\n");
        return 1;
    }

    // create dbgpr_thread, this thread supports process_data calls to dbgpr;
    // this is needed because process_data should avoid calls that may take a 
    //  long time to complete
    pthread_create(&tid_dbgpr_thread, NULL, dbgpr_thread, NULL);

    // initialize get data from either a wav file or the 4 channel microphone;
    // this initialization starts periodic callbacks to process_data()
    if (data_src == DATA_SRC_FILE) {
        if (init_get_data_from_file(data_src_name) < 0) {
            printf("ERROR: init_get_data_from_file failed\n");
            return 1;
        }
    } else {
        if (init_get_data_from_mic(data_src_name) < 0) {
            printf("ERROR: init_get_data_from_mic failed\n");
            return 1;
        }
    }

    // command loop
    char cmd[200];
    while (printf("> "), fgets(cmd, sizeof(cmd), stdin) != NULL) {
        cmd[strcspn(cmd, "\n")] = '\0';
        printf("GOT CMD: %s\n", cmd);
        if (strcmp(cmd, "q") == 0) {
            break;
        }
    }

    // program terminating:
    // - set prog_terminating flag, and
    // - wait for all threads to exit
    printf("terminating\n");
    prog_terminating = true;
    if (tid_leds_thread) pthread_join(tid_leds_thread, NULL);
    if (tid_get_data_from_file_thread) pthread_join(tid_get_data_from_file_thread, NULL);
    if (tid_get_data_from_mic_thread) pthread_join(tid_get_data_from_mic_thread, NULL);
    if (tid_dbgpr_thread) pthread_join(tid_dbgpr_thread, NULL);

    // done
    return 0;
}

// -----------------  GET_DATA FROM MIC  -----------------------------------

static int init_get_data_from_mic(char *dev_name)
{
    if (pa_init() < 0) {
        printf("ERROR: pa_init failed\n");
        return 1;
    }

    // create get_data_from_mic_thread
    pthread_create(&tid_get_data_from_mic_thread, NULL, get_data_from_mic_thread, dev_name);

    // xxx wait for up to 3 secs for process_data_cb to be called
    //     or for pa_record2 to return an error
    return 0;
}

static void * get_data_from_mic_thread(void *cx)
{
    char *dev_name = cx;

    if (pa_record2(dev_name, MAX_CHAN, SAMPLE_RATE, process_data, NULL, 0) < 0) {
        printf("ERROR: pa_record2 failed\n");
        return NULL;
    }
    return NULL;
}

// -----------------  GET DATA FROM FILE  ----------------------------------

static float *file_data;
static int    file_max_chan;
static int    file_max_frames;

static int init_get_data_from_file(char *file_name)
{
    int file_max_data, file_sample_rate;

    // read file
    if (strstr(file_name, ".wav") == NULL) {
        printf("ERROR: file_name must have '.wav' extension\n");
        return -1;
    }
    if (sf_read_wav_file(file_name, &file_data, &file_max_chan, &file_max_data, &file_sample_rate) < 0) {
        printf("ERROR: sf_read_wav_file %s failed\n", file_name);
        return -1;
    }
    if (file_sample_rate != SAMPLE_RATE) {
        printf("ERROR: file sample_rate=%d, must be %d\n", file_sample_rate, SAMPLE_RATE);
        return -1;
    }
    if (file_max_chan != MAX_CHAN) {
        printf("ERROR: file max_chan=%d, must be %d\n", file_max_chan, MAX_CHAN);
        return -1;
    }
    file_max_frames = file_max_data / file_max_chan;

    // create get_data_from_file_thread
    pthread_create(&tid_get_data_from_file_thread, NULL, get_data_from_file_thread, NULL);

    // return success
    return 0;
}

static void *get_data_from_file_thread(void *cx)
{
    int      frame_idx=0, i;
    uint64_t time_next_us;

    // loop, passing file data to routine for procesing
    time_next_us = microsec_timer();
    while (true) {
        // provide 48 samples to the process_data routine
        for (i = 0; i < 48; i++) {
            if (process_data(&file_data[frame_idx*file_max_chan], NULL) < 0) {
                return NULL;
            }
            frame_idx = (frame_idx + 1) % file_max_frames;
        }

        // sleep until time_next_us
        time_next_us += (48 * 1000000) / SAMPLE_RATE;
        sleep_until(time_next_us);
    }

    return NULL;
}

// -----------------  PROCESS 4 CHANNEL AUDIO DATA --------------------------

#if 0
//      Cross correlate data for mics:
//      - AX and AY
//      - BX and BY
//      ---------------------
//      |BX               AY|
//      |                   |
//      |                   |
//      |     RESPEAKER     |
//      |                   |
//      |AX               BY|
//      ---------------------
#define CCA_MICX     0
#define CCA_MICY     2
#define CCB_MICX     1
#define CCB_MICY     3
#define ANGLE_OFFSET 45
#else
//      Cross correlate data for mics:
//      - AX/BX and AY
//      - AX/BX and BY
//      ---------------------
//      |AY               --|
//      |                   |
//      |                   |
//      |     RESPEAKER     |
//      |                   |
//      |AX/BX            BY|
//      ---------------------
#define CCA_MICX     0
#define CCA_MICY     1
#define CCB_MICX     0
#define CCB_MICY     3
#define ANGLE_OFFSET 0
#endif

#define N (15)   // N is half the number of cross correlations 

#define MAX_FRAME            (MS_TO_FRAMES(500))
#define MIN_AMP              (0.0002)
#define MIN_INTEGRAL         (1.0)

#define WINDOW_DURATION      ((double)MAX_FRAME / SAMPLE_RATE)
#define MS_TO_FRAMES(ms)     (SAMPLE_RATE * (ms) / 1000)
#define FRAMES_TO_MS(frames) (1000 * (frames) / SAMPLE_RATE)

static int process_data(const float *frame, void *cx)
{
    #define DATA(_chan,_offset) \
        ( data [ _chan ] [ data_idx+(_offset) >= 0 ? data_idx+(_offset) : data_idx+(_offset)+MAX_FRAME ] )

    static double   data[MAX_CHAN][MAX_FRAME];
    static int      data_idx;

    // if this program is returning, return -1, which causes the caller to stop recording
    if (prog_terminating) {
        return -1;
    }

    // increment data_idx, and frame_cnt
    data_idx = (data_idx + 1) % MAX_FRAME;
    frame_cnt++;

    // print the frame rate once per sec
    if (debug & DEBUG_FRAME_RATE) {
        static double   time_last_frame_rate_print_us;
        static uint64_t frame_cnt_last_print;
        uint64_t time_now_us = microsec_timer();
        if (time_last_frame_rate_print_us == 0) {
            time_last_frame_rate_print_us = time_now_us;
        }
        if (time_now_us - time_last_frame_rate_print_us >= 1000000) {
            dbgpr("FC=%" PRId64 ": FRAME RATE = %d\n", 
                  frame_cnt, (int)(frame_cnt-frame_cnt_last_print));
            frame_cnt_last_print = frame_cnt;
            time_last_frame_rate_print_us = time_now_us;
        }
    }

    // apply high pass filter to the input frame and store in 'data' array
    // xxx use the ex filter
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
    if (debug & DEBUG_AMP_VERBOSE) {
        dbgpr("FC=%" PRId64 ": amp_sum=%0.6f  amp=%10.6f\n", frame_cnt, amp_sum, amp);
    }

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
            if (debug & DEBUG_AMP) {
                dbgpr("FC=%" PRId64 ": START_FRAME_CNT=%" PRId64 "\n", frame_cnt, start_frame_cnt);
            }
        }
    } else {
        integral += squared(DATA(0,0));
        if (frame_cnt == start_frame_cnt + MS_TO_FRAMES(150)) {
            trigger_integral = integral;
            if (integral < MIN_INTEGRAL) {
                start_frame_cnt = 0;
                if (debug & DEBUG_AMP) {
                    dbgpr("FC=%" PRId64 ": CANCELLING, integral=%0.3f MIN_INTEGRAL=%0.3f\n",
                          frame_cnt, integral, MIN_INTEGRAL);
                }
            } else {
                if (debug & DEBUG_AMP) {
                    dbgpr("FC=%" PRId64 ": ACCEPTING, integral=%0.3f MIN_INTEGRAL=%0.3f\n",
                          frame_cnt, integral, MIN_INTEGRAL);
                }
            }
        } else if (frame_cnt == start_frame_cnt + MS_TO_FRAMES(480)) {
            analyze = true;
            start_frame_cnt = 0;
        }
    }

    // if there is not sound data to analyze then return
    if (analyze == false) {
        return 0;
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

    // The deviation of the max cross correlation from center is
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
        dbgpr("FC=%" PRId64 ": ANALYZE SOUND - ERROR max_cca_idx=%d max_ccb_idx=%d\n", 
              frame_cnt, max_cca_idx, max_ccb_idx);
        return 0;
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

    // make sound angle available to the led_thread
    leds.angle = angle;
    __sync_synchronize();
    leds.angle_frame_cnt = frame_cnt;

    // debug prints
    if (debug & DEBUG_ANALYZE_SOUND) {
        double time_now_secs = FRAMES_TO_MS(frame_cnt)/1000.;
        dbgpr("FC=%" PRId64 ": ANALYZE SOUND - trigger_integral=%0.3f %0.3f  integral=%0.3f  intvl=%0.3f ... %0.3f\n", 
              frame_cnt, trigger_integral, MIN_INTEGRAL, integral, 
              time_now_secs-WINDOW_DURATION, time_now_secs);
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

    // continue receiving sound data
    return 0;
}

// -----------------  PROCESS DATA - DEBUG PRINT SUPPORT  ------------------

// The process_data routine should not call printf directly, because that could
// cause unpredicatable execution delays. Instead, process_data calls dbgpr(),
// which prints to dbgpr_buff. And, the dbgpr_thread performes the printf.

#define MAX_DBGPR     10000
#define MAX_DBGPR_STR 150

static char dbgpr_buff[MAX_DBGPR][MAX_DBGPR_STR];
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
    cnt = vsnprintf(dbgpr_buff[idx], MAX_DBGPR_STR, fmt, ap);
    if (cnt >= MAX_DBGPR_STR) {
        dbgpr_buff[idx][MAX_DBGPR_STR-2] = '\n';
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
            if (prog_terminating) return NULL;
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

uint64_t microsec_timer(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC,&ts);
    return  ((uint64_t)ts.tv_sec * 1000000) + ((uint64_t)ts.tv_nsec / 1000);
}

// -----------------  LEDS  ------------------------------------------------

#define MAX_LEDS 12

static int leds_init(void)
{
    // this enables Vcc for the Respeaker LEDs
    if (wiringPiSetupGpio() == -1) {
        printf("ERROR: wiringPiSetupGpio failed\n");
        return -1;
    }
    pinMode (5, OUTPUT);
    digitalWrite(5, 1);

    // init apa102 leds support code
    if (apa102_init(MAX_LEDS) < 0) {
        printf("ERROR: apa102_init\n");
        return -1;
    }

    // create thread to update leds
    pthread_create(&tid_leds_thread, NULL, leds_thread, NULL);;
    return 0;
}

static void * leds_thread(void * cx) 
{
    int i, led_a, led_b;

    static struct {
        unsigned int rgb;
        int brightness;
    } desired[MAX_LEDS], current[MAX_LEDS];

    while (true) {
        // if program terminating then break
        if (prog_terminating) {
            break;
        }

        // determine desired led values
        for (i = 0; i < MAX_LEDS; i++) {
            desired[i].rgb = LED_LIGHT_BLUE;
            desired[i].brightness = 25;
        }

        if (leds.angle_frame_cnt != 0 && FRAMES_TO_MS(frame_cnt-leds.angle_frame_cnt) < 2000) {
            convert_angle_to_led_num(leds.angle, &led_a, &led_b);
            desired[led_a].rgb = LED_WHITE;
            desired[led_a].brightness = 100;
            if (led_b != -1) {
                desired[led_b].rgb = LED_WHITE;
                desired[led_b].brightness = 100;
            }
        }

        // if desired led values are different than current then update
        if (memcmp(&desired, &current, sizeof(current)) != 0) {
            for (i = 0; i < MAX_LEDS; i++) {
                apa102_set_led(i, desired[i].rgb, desired[i].brightness);
            }
            apa102_show_leds(31);
            memcpy(current, desired, sizeof(current));
        }

        // sleep 100 ms
        usleep(100000);
    }

    // clear leds and terminate thread
    apa102_set_all_leds_off();
    apa102_show_leds(0);
    return NULL;
}

static void convert_angle_to_led_num(double angle, int *led_a, int *led_b)
{
#if 1
    *led_a = nearbyint( normalize_angle(angle) / (360/MAX_LEDS) );
    if (*led_a == 12) *led_a = 0;
    *led_b = -1;
#else
    int tmp = nearbyint( normalize_angle(angle) / (360/(2*MAX_LEDS)) );

    if ((tmp & 1) == 0) {
        *led_a = tmp/2;
        *led_b = -1;
    } else {
        *led_a = tmp/2;
        *led_b = *led_a + 1;
    }

    if (*led_a == MAX_LEDS) *led_a = 0;
    if (*led_b == MAX_LEDS) *led_b = 0;
#endif
}
