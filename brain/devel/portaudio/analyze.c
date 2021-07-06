// xxx todo:
// - try cross config

// xxx ideas to improve relieability
// - discard outliers
// - discard if sum squares of deviations too small
// - discard if there is not a clear peak to cross corr
// - do a larger poly_fit

// NOTES:
// - to record a wav file that can be used with this programs -f option:
//     arecord -D sysdefault:CARD=seeed4micvoicec -r 48000 -c 4 file.wav
// - to play the wav file
//     aplay -D sysdefault:CARD=Device file.wav
// - to list the capture hardware devices, and PCMs
//     arecord -l
//     arecord -L
// - to set speaker volume, run curses program
//     alsamixer


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

#define SAMPLE_RATE         48000
#define MAX_CHAN            4

#define DATA_SRC_MIC        1
#define DATA_SRC_FILE       2

//
// typedefs
//

typedef struct {
    double   angle;
    uint64_t angle_frame_cnt;
} doa_t;  // direction of arrival

//
// variables
//

static bool      prog_terminating;

static pthread_t tid_dbgpr_thread;
static pthread_t tid_leds_thread;
static pthread_t tid_get_data_from_mic_thread;
static pthread_t tid_get_data_from_file_thread;
static pthread_t tid_get_data_from_file_thread2;

static uint64_t  frame_cnt;
static doa_t     doa;

//
// params
//

static int start_sound_block_amp_limit  = 500;
static int debug = 1; 

#define MAX_PARAMS (sizeof(params)/sizeof(params[0]))

static struct {
    char *name;
    int  *value;
} params[] = {
    { "amp",   &start_sound_block_amp_limit },
    { "debug", &debug },
                };

//
// prototpes
//

static int init_get_data_from_mic(char *mic_dev_name);
static void *get_data_from_mic_thread(void *cx);
static int process_mic_frame(const float *frame, void *cx);

static int init_get_data_from_file(char *file_name, char *spkr_dev_name);
static void *get_data_from_file_thread(void *cx);
static int play_get_frame(float *ret_spkr_data, void *cx);
static void *get_data_from_file_thread2(void *cx);

static void process_frame(const float *frame);
static void dbgpr_frame_rate(void);

static int dbgpr_init(void);
static void dbgpr(char *fmt, ...) __attribute__ ((format (printf, 1, 2)));
static void *dbgpr_thread(void *cx);

static void sleep_until(uint64_t time_next_us);
static double squared(double v);
static double normalize_angle(double angle);
static char *stars(double v, double max_v, int max_stars, char *s);
static uint64_t microsec_timer(void);
static double min_doubles(double *x, int n, int *min_idx_arg) __attribute__ ((unused));
static double max_doubles(double *x, int n, int *max_idx_arg);

static int leds_init(void);
static void *leds_thread(void * cx);
static void convert_angle_to_led_num(double angle, int *led_a, int *led_b);

// -----------------  MAIN  ------------------------------------------------

int main(int argc, char **argv)
{
    int   data_src      = DATA_SRC_MIC;
    char *mic_dev_name  = "seeed-4mic-voicecard";
    char *file_name     = NULL;
    char *spkr_dev_name = NULL;

    // use line buffered stdout
    setlinebuf(stdout);

   #define USAGE \
    "usage: analyze [-d mic_dev_name] [-f file_name[,spkr_dev_name]]\n" \
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
            data_src = DATA_SRC_FILE;
            file_name = strtok(optarg, ",");
            spkr_dev_name = strtok(NULL, " ");
            break;
        case 'd':
            data_src = DATA_SRC_MIC;
            mic_dev_name = optarg;
            break;
        case 'h':
            printf("%s\n", USAGE);
            return 0;
            break;
        default:
            return 1;
        };
    }

    // print args
    if (data_src == DATA_SRC_FILE) {
        printf("DATA_SRC_FILE: file_name=%s  spkr_dev_name=%s\n", 
               file_name, spkr_dev_name);
    } else {
        printf("DATA_SRC_MIC:  mic_dev_name=%s\n", mic_dev_name);
    }

    // init leds, which will be used to indicate the direction of the sound
    if (leds_init() < 0) {
        printf("ERROR: leds_int failed\n");
        return 1;
    }

    // init debug print 
    if (dbgpr_init() < 0) {
        printf("ERROR: dbgpr_init failed\n");
        return 1;
    }

    // initialize get data from either a wav file or the 4 channel microphone;
    // this initialization starts periodic callbacks to process_frame()
    if (data_src == DATA_SRC_FILE) {
        if (init_get_data_from_file(file_name, spkr_dev_name) < 0) {
            printf("ERROR: init_get_data_from_file failed\n");
            return 1;
        }
    } else {
        if (init_get_data_from_mic(mic_dev_name) < 0) {
            printf("ERROR: init_get_data_from_mic failed\n");
            return 1;
        }
    }

    // debug command loop
    char cmdline[200];
    while (printf("> "), fgets(cmdline, sizeof(cmdline), stdin) != NULL) {
        // remove trailing newline char, 
        // use strtok to get the cmd field, and
        // if no cmd then continue
        cmdline[strcspn(cmdline, "\n")] = '\0';
        char *cmd = strtok(cmdline, " ");
        if (cmd == NULL) {
            continue;
        }

        // process the cmd
        if (strcmp(cmd, "q") == 0) {
            break;
        } else if (strcmp(cmd, "set") == 0) {
            char *name = strtok(NULL, " ");
            char *value_str = strtok(NULL, " ");
            double value;
            int i;

            if (name == NULL) {
                for (int i = 0; i < MAX_PARAMS; i++) {
                    printf("%-8s = %d\n", params[i].name, *params[i].value);
                }
                continue;
            }

            if ((value_str == NULL) || (sscanf(value_str, "%lf", &value) != 1)) {
                printf("ERROR: set - missing value\n");
                continue;
            }

            for (i = 0; i < MAX_PARAMS; i++) {
                if (strcmp(name, params[i].name) == 0) {
                    *params[i].value = value;
                    break;
                }
            }
            if (i == MAX_PARAMS) {
                printf("ERROR: set - '%s' not found\n", name);
            }
        } else {
            printf("ERROR: invalid cmd '%s'\n", cmd);
        }
    }

    // program terminating:
    // - set prog_terminating flag, and
    // - wait for all threads to exit
    printf("terminating\n");
    prog_terminating = true;
    if (tid_leds_thread) pthread_join(tid_leds_thread, NULL);
    if (tid_get_data_from_file_thread) pthread_join(tid_get_data_from_file_thread, NULL);
    if (tid_get_data_from_file_thread2) pthread_join(tid_get_data_from_file_thread2, NULL);
    if (tid_get_data_from_mic_thread) pthread_join(tid_get_data_from_mic_thread, NULL);
    if (tid_dbgpr_thread) pthread_join(tid_dbgpr_thread, NULL);

    // done
    return 0;
}

// -----------------  GET_DATA FROM MIC  -----------------------------------

static int init_get_data_from_mic(char *mic_dev_name)
{
    // init portaudio
    if (pa_init() < 0) {
        printf("ERROR: pa_init failed\n");
        return 1;
    }

    // create get_data_from_mic_thread
    pthread_create(&tid_get_data_from_mic_thread, NULL, get_data_from_mic_thread, mic_dev_name);

    // return success; 
    // if get_data_from_mic_thread it will exit()
    return 0;
}

static void * get_data_from_mic_thread(void *cx)
{
    char *mic_dev_name = cx;

    // call pa_record2 to initiate periodic callbacks to process_mic_frame, 
    // with 4 channel frame data from the microphone
    if (pa_record2(mic_dev_name, MAX_CHAN, SAMPLE_RATE, process_mic_frame, NULL, 0) < 0) {
        printf("ERROR: pa_record2 failed\n");
        exit(1);
    }
 
    // terminate thread
    return NULL;
}

static int process_mic_frame(const float *frame, void *cx)
{
    // check if this program is terminating
    if (prog_terminating) {
        return -1;
    }

    // process the frame
    process_frame(frame);

    // return status to continue
    return 0;
}

// -----------------  GET DATA FROM FILE  ----------------------------------

// If spkr_dev_name is provided the data is both played to the spearker and 
// analyzed to determine sound direction.
//
// Otherwise, just the sound direction is analyzed.

static float *file_data;
static int    file_max_chan;
static int    file_max_frames;

static int init_get_data_from_file(char *file_name, char *spkr_dev_name)
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

    // if spkr_dev_name is provided then
    //   both play the file to the speaker and analyze the sound direction
    // else
    //   just analyze the sound direction of file contents
    // endif
    if (spkr_dev_name) {
        // init portaudio
        if (pa_init() < 0) {
            printf("ERROR: pa_init failed\n");
            return -1;
        }

        // create get_data_from_file_thread, this thread will:
        // - play the file to the speaker, and
        // - perform sound direction analysis
        pthread_create(&tid_get_data_from_file_thread, NULL, get_data_from_file_thread, spkr_dev_name);
    } else {
        // create get_data_from_file_thread2, this thread will:
        // - perform sound direction analysis
        pthread_create(&tid_get_data_from_file_thread2, NULL, get_data_from_file_thread2, NULL);
    }

    // return success
    return 0;
}

static void *get_data_from_file_thread(void *cx)
{
    char *spkr_dev_name = cx;

    // call pa_play2 which will schedule the play_get_frame periodic callback, 
    // the callback will:
    // - return a 1 channel frame of file data, to be played on the speaker
    // - call process_frame with a 4 channel frame of file data, for sound
    //   direction analysis
    if (pa_play2(spkr_dev_name, 1, SAMPLE_RATE, play_get_frame, NULL) < 0) {
        printf("ERROR: pa_play2 failed\n");
        exit(1);
    }

    // terminate thread
    return NULL;
}

static int play_get_frame(float *ret_spkr_data, void *cx)
{
    static int frame_idx;
    float *frame;

    // check if this program is terminating
    if (prog_terminating) {
        return -1;
    }

    // get ptr to the next frame of file_data
    frame = &file_data[frame_idx*file_max_chan];
    frame_idx = (frame_idx + 1) % file_max_frames;

    // process the data through the sound localization process_frame routine
    process_frame(frame);

    // return the frame to caller, so it will be played on the speaker
    *ret_spkr_data = frame[0];

    // continue playing
    return 0;
}

static void *get_data_from_file_thread2(void *cx)
{
    int      frame_idx=0, i;
    uint64_t time_next_us;

    // loop, passing file data to routine for procesing
    time_next_us = microsec_timer();
    while (true) {
        // check if this program is terminating
        if (prog_terminating) {
            break;
        }
        
        // provide 48 samples to the process_frame routine
        for (i = 0; i < 48; i++) {
            process_frame(&file_data[frame_idx*file_max_chan]);
            frame_idx = (frame_idx + 1) % file_max_frames;
        }

        // sleep until time_next_us
        time_next_us += (48 * 1000000) / SAMPLE_RATE;
        sleep_until(time_next_us);
    }

    // terminate thread
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

// N is half the number of cross correlations 
#define N                       (15)   

// duration of a sound block analysis
#define MAX_FRAME               (MS_TO_FRAMES(500))

#define WINDOW_DURATION         ((double)MAX_FRAME / SAMPLE_RATE)
#define MS_TO_FRAMES(ms)        (SAMPLE_RATE * (ms) / 1000)
#define FRAMES_TO_MS(frames)    (1000 * (frames) / SAMPLE_RATE)
#define FRAME_CNT_TO_TIME(fc)   ((double)(fc) / SAMPLE_RATE)

#define DATA(_chan,_offset) \
    ( data [ _chan ] [ data_idx+(_offset) >= 0 ? data_idx+(_offset) : data_idx+(_offset)+MAX_FRAME ] )
#define AH(x) ( &amp_history[ ahidx+(x) >= 0 ? ahidx+(x) : ahidx+(x)+100 ] )

static void process_frame(const float *frame)
{
    static double data[MAX_CHAN][MAX_FRAME];
    static int    data_idx;

    static double cca[2*N+1];  
    static double ccb[2*N+1];

    double amp;
    static struct amp_history_s {
        double   amp;
        uint64_t frame_cnt;
        bool     flag;
    } amp_history[200];
    static int ahidx;

    static uint64_t start_sound_block_frame_cnt;
    static double   start_sound_block_amp_sum;

    // increment data_idx, and frame_cnt
    data_idx = (data_idx + 1) % MAX_FRAME;
    frame_cnt++;

    // debug print the frame rate once per sec
    if (0) dbgpr_frame_rate();

    // for each mic channel, copy the input frame to data arrays
    for (int chan = 0; chan < MAX_CHAN; chan++) {
        DATA(chan,0) = frame[chan];
    }

    // compute cross correlations for the 2 pairs of mic channels
    for (int i = -N; i <= N; i++) {
        cca[i+N] += DATA(CCA_MICX,-N) * DATA(CCA_MICY,-(N+i))  -
                    DATA(CCA_MICX,-((MAX_FRAME-1)-N)) * DATA(CCA_MICY,-((MAX_FRAME-1)-N+i));
        ccb[i+N] += DATA(CCB_MICX,-N) * DATA(CCB_MICY,-(N+i))  -
                    DATA(CCB_MICX,-((MAX_FRAME-1)-N)) * DATA(CCB_MICY,-((MAX_FRAME-1)-N+i));
    }

    // compute average amp over the past 10 ms, this is compted using mic chan 0
    static double amp_sum;
    amp_sum += (squared(DATA(0,0)) - squared(DATA(0,-MS_TO_FRAMES(10))));
    amp = amp_sum / MS_TO_FRAMES(10);

    // the remaining code is executed every 10 ms;
    // so, return here if frame_cnt is not a multiple of the number of frames in 10 ms
    if ((frame_cnt % MS_TO_FRAMES(10)) != 0) {
        return;
    }

    // save amp in the amp_history circular buffer;
    // note - amp is scaled by 1e5 to make the values more convenient
    amp *= 1e5;
    ahidx = (ahidx + 1) % 100;
    AH(0)->amp       = amp;
    AH(0)->frame_cnt = frame_cnt;
    AH(0)->flag      = false;

    // if start of sound block is not set, and the 
    // sum of the 3 most recent amp values exceeds minimum limit
    // then the start of a sound block has been found
    if ((start_sound_block_frame_cnt == 0) &&
        (AH(0)->amp + AH(-1)->amp + AH(-2)->amp > start_sound_block_amp_limit))
    {
        start_sound_block_frame_cnt = frame_cnt;
        start_sound_block_amp_sum = AH(0)->amp + AH(-1)->amp + AH(-2)->amp;
        AH(0)->flag = true;
        AH(-1)->flag = true;
        AH(-2)->flag = true;
    }
            
    // if the frame_cnt has reached the end of the sound block then
    //   debug print the sound block amplitude values (these are in 10 ms intervals), and
    //   set the analyze flag
    bool analyze = false;
    if (start_sound_block_frame_cnt != 0 && frame_cnt - start_sound_block_frame_cnt >= MS_TO_FRAMES(400)) {
        dbgpr("\n");
        dbgpr("============================================================================================\n");
        dbgpr("AMP: LIMIT=%d  SUM=%0.0f\n", 
              start_sound_block_amp_limit, start_sound_block_amp_sum);

        for (int i = -49; i <= 0; i++) {
            char s[200];
            dbgpr("%8.3f: %-3.0f %c : %s\n",
                FRAME_CNT_TO_TIME(AH(i)->frame_cnt),
                AH(i)->amp,
                AH(i)->flag ? 'X' : ' ',
                stars(AH(i)->amp, 1000, 100, s));
        }

        start_sound_block_frame_cnt = 0;
        start_sound_block_amp_sum = 0;
        analyze = true;
    }

    // if there is not a sound block to analyze then return
    if (analyze == false) {
        return;
    }

    // -----------------------------------------------------
    // the following code blocks determine the angle to the sound
    // 
    //                         0
    //                         ^
    //              270 <- RESPEAKER -> 90
    //                         v
    //                        180
    // -----------------------------------------------------

    int           max_cca_idx, max_ccb_idx;
    double        max_cca, max_ccb, coeffs[3], cca_x, ccb_x, angle;
    static double x[2*N+1];
    if (x[0] == 0) { // init on first call
        for (int i = -N; i <= N; i++) x[i+N] = i;
    }

    // Each of the 2 pairs of mics has 2*N+1 cross correlation results that
    // is computed by the code near the top of this routine.
    // Determine the max cross-corr value for the sets of cross correlations
    // performed for the 2 pairs of mics.
    max_cca = max_doubles(cca, 2*N+1, &max_cca_idx);
    max_cca_idx -= N;
    max_ccb = max_doubles(ccb, 2*N+1, &max_ccb_idx);
    max_ccb_idx -= N;

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
        dbgpr("%8.3f: ANALYZE SOUND - ERROR max_cca_idx=%d max_ccb_idx=%d\n", 
              FRAME_CNT_TO_TIME(frame_cnt), max_cca_idx, max_ccb_idx);
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

    // determine the direction of sound arrival angle
    angle = atan2(ccb_x, cca_x) * (180/M_PI);
    angle = normalize_angle(angle + ANGLE_OFFSET);

    // make the direction of sound arrival angle available to the led_thread
    doa.angle = angle;
    __sync_synchronize();
    doa.angle_frame_cnt = frame_cnt;

    // debug prints results of sound direction analysis
    if (1) {
        double tnow = FRAME_CNT_TO_TIME(frame_cnt);
        dbgpr("%8.3f: ANALYZE SOUND - intvl=%0.3f ... %0.3f\n", 
              FRAME_CNT_TO_TIME(frame_cnt), tnow-WINDOW_DURATION, tnow);
        for (int i = -N; i <= N; i++) {
            char s1[100], s2[100];
            dbgpr("%3d: %5.1f %-30s - %5.1f %-30s\n",
                   i,
                   cca[i+N], stars(cca[i+N], max_cca, 30, s1),
                   ccb[i+N],  stars(ccb[i+N], max_ccb, 30, s2));
        }
        dbgpr("       LARGEST AT %-10.3f                 LARGEST AT %-10.3f\n", cca_x, ccb_x);
        dbgpr("       SOUND ANGLE = %0.1f degs *****\n", angle);
    }
}

static void dbgpr_frame_rate(void)
{
    static double   time_last_frame_rate_print_us;
    static uint64_t frame_cnt_last_print;
    uint64_t time_now_us = microsec_timer();

    if (time_last_frame_rate_print_us == 0) {
        time_last_frame_rate_print_us = time_now_us;
    }
    if (time_now_us - time_last_frame_rate_print_us >= 1000000) {
        dbgpr("%8.3f: FRAME RATE = %d\n", 
              FRAME_CNT_TO_TIME(frame_cnt), (int)(frame_cnt-frame_cnt_last_print));
        frame_cnt_last_print = frame_cnt;
        time_last_frame_rate_print_us = time_now_us;
    }
}

// -----------------  PROCESS DATA - DEBUG PRINT SUPPORT  ------------------

// The process_frame routine should not call printf directly, because that could
// cause unpredicatable execution delays. Instead, process_frame calls dbgpr(),
// which prints to dbgpr_buff. And, the dbgpr_thread performes the printf.

#define MAX_DBGPR     10000
#define MAX_DBGPR_STR 150

static char dbgpr_buff[MAX_DBGPR][MAX_DBGPR_STR];
static volatile uint64_t prints_produced;
static volatile uint64_t prints_consumed;
static FILE *fp_dbgpr;

static int dbgpr_init(void)
{
    // open dbgpr logfile, and set linebuffered
    fp_dbgpr = fopen("analyze.log", "w");
    if (fp_dbgpr == NULL) {
        printf("ERROR: open analyze.log, %s\n", strerror(errno));
        return -1;
    }
    setlinebuf(fp_dbgpr);

    // create dbgpr_thread, this thread supports process_frame calls to dbgpr;
    // this is needed because process_frame should avoid calls that may take a 
    //  long time to complete
    pthread_create(&tid_dbgpr_thread, NULL, dbgpr_thread, NULL);

    // success
    return 0;
}

static void dbgpr(char *fmt, ...)
{
    int idx, cnt;
    va_list ap;

    // if debug not enabled then return
    if (debug == 0) {
        return;
    }

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

    fprintf(fp_dbgpr, "dbgpr_thread starting\n");

    while (true) {
        while (prints_produced == prints_consumed) {
            if (prog_terminating) {
                goto done;
            }
            usleep(10000);
        }

        while (prints_produced > prints_consumed) {
            idx = (prints_consumed % MAX_DBGPR);
            fprintf(fp_dbgpr, "%s", dbgpr_buff[idx]);
            __sync_synchronize();

            prints_consumed++;
        }
    }

done:
    fprintf(fp_dbgpr, "dbgpr_thread terminating\n");
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

static double min_doubles(double *x, int n, int *min_idx_arg)
{
    double min = x[0];
    int    min_idx = 0;

    for (int i = 1; i < n; i++) {
        if (x[i] < min) {
            min = x[i];
            min_idx = i;
        }
    }

    if (min_idx_arg) {
        *min_idx_arg = min_idx;
    }

    return min;
}

static double max_doubles(double *x, int n, int *max_idx_arg)
{
    double max = x[0];
    int    max_idx = 0;

    for (int i = 1; i < n; i++) {
        if (x[i] > max) {
            max = x[i];
            max_idx = i;
        }
    }

    if (max_idx_arg) {
        *max_idx_arg = max_idx;
    }

    return max;
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

        // if an direction of arrival has been published within 1000 ms 
        // then convert the direction to led(s), and set the leds to white
        if (doa.angle_frame_cnt != 0 && FRAMES_TO_MS(frame_cnt-doa.angle_frame_cnt) < 1000) {
            convert_angle_to_led_num(doa.angle, &led_a, &led_b);
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
    // this ifdef selects between returning one or two led_nums to
    // represent the angle
#if 0
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
