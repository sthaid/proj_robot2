#include <common.h>

//
// defines
//

#define MAX_RECORDING (60*16000)

//
// variables
//

static bool  end_program;
static short recording[MAX_RECORDING];
static int   recording_idx;

//
// prototypes
//

static void initialize(void);
static void sig_hndlr(int sig);
static int proc_mic_data(short *frame);
static void set_leds(unsigned int color, int brightness, double doa);

// -----------------  MAIN  ------------------------------------------------------

int main(int argc, char **argv)
{
    // initialize
    log_init(NULL, false, false);
    INFO("INITIALIZING\n")
    initialize();

    // run
    INFO("PROGRAM RUNNING\n");
    t2s_play("program running");
    set_leds(LED_BLUE, 50, -1);

    // wait for end_pgm
    while (!end_program) {
        usleep(100000);
    }
    
    // program is terminating
    INFO("PROGRAM TERMINATING\n")
    t2s_play("program terminating");
    set_leds(LED_OFF, 0, -1);

    // return success
    return 0;
}

static void initialize(void)
{
    uint64_t secs_since_boot;

    // register for SIGINT and SIGTERM
    static struct sigaction act;
    act.sa_handler = sig_hndlr;
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGTERM, &act, NULL);

    // xxx
    secs_since_boot = microsec_timer()/1000000;
    if (secs_since_boot < 20) {
        INFO("secs_since_boot=%lld, sleeping 20 secs\n", secs_since_boot);
        sleep(20);
    }

    // xxx
    if (wiringPiSetupGpio() != 0) {
        FATAL("wiringPiSetupGpio failed\n");
    }

    // xxx
    srandom(time(NULL));

    // xxx
    misc_init();
    wwd_init();
    t2s_init();
    s2t_init();
    doa_init();
    leds_init();
    sf_init();
    db_init("db.dat", true, GB);
    proc_cmd_init();
    audio_init(proc_mic_data);
    body_init();
}

static void sig_hndlr(int sig)
{
    brain_end_program();
}

// -----------------  PUBLIC ROUTINES  -------------------------------------------

void brain_end_program(void)
{
    end_program = true;
}

void brain_restart_program(void)
{
    system("sudo systemctl restart robot-brain &");
}

void brain_get_recording(short *data, int max)
{
    int ri = recording_idx;

    if (ri-max >= 0) {
        memcpy(data, recording+(ri-max), max*sizeof(short));
    } else {
        int tmp = -(ri-max);
        memcpy(data, recording+(MAX_RECORDING-tmp), tmp*sizeof(short));
        memcpy(data+tmp, recording, (max-tmp)*sizeof(short));
    }
}

// -----------------  PROCESS MIC DATA FRAME  ------------------------------------

// called at sample rate 48000
static int proc_mic_data(short *frame)
{
    #define STATE_WAITING_FOR_WAKE_WORD  0
    #define STATE_RECEIVING_CMD          1
    #define STATE_PROCESSING_CMD         2
    #define STATE_COMPLETED_CMD_OKAY     3
    #define STATE_COMPLETED_CMD_ERROR    4

    static int    state = STATE_WAITING_FOR_WAKE_WORD;
    static double doa;

    // supply the frame for doa analysis, frame is 4 shorts
    doa_feed(frame);

    // discard 2 out of 3 frames, so the sample rate for the code following is 16000
    static int discard_cnt;
    if (++discard_cnt < 3) {
        return 0;
    }
    discard_cnt = 0;

    // the sound value used for the remaining of this routine is the value from mic chan 0;
    // low pass filter is used to eliminate high frequency background noise
#if 1
    static double cx[2];
    short sound_val = 5 * low_pass_filter_ex(frame[0], cx, 2, 0.90);
#else
    short sound_val = frame[0];
#endif

    // save sound recording so it can be played to test audio quality
    recording[recording_idx] = sound_val;
    recording_idx = (recording_idx == MAX_RECORDING-1 ? 0 : recording_idx+1);

    // process mic data state machine
    switch (state) {
    case STATE_WAITING_FOR_WAKE_WORD: {
        if (wwd_feed(sound_val) & WW_KEYWORD_MASK) {
            state = STATE_RECEIVING_CMD;
            doa = doa_get();
            set_leds(LED_WHITE, 100, doa);
        }
        break; }
    case STATE_RECEIVING_CMD: {
        char *transcript = s2t_feed(sound_val);
        if (transcript) {
            if (strcmp(transcript, "TIMEDOUT") == 0) {
                free(transcript);
                set_leds(LED_BLUE, 50, -1);
                state = STATE_WAITING_FOR_WAKE_WORD;
                break;
            }
            proc_cmd_execute(transcript, doa);
            state = STATE_PROCESSING_CMD;
        }
        break; }
    case STATE_PROCESSING_CMD: {
        bool succ;
        if (proc_cmd_in_progress(&succ) == false) {
            state = (succ ? STATE_COMPLETED_CMD_OKAY : STATE_COMPLETED_CMD_ERROR);
            break;
        }
        if (wwd_feed(sound_val) & WW_TERMINATE_MASK) {
            proc_cmd_cancel();
        }
        break; }
    case STATE_COMPLETED_CMD_OKAY: {
        set_leds(LED_BLUE, 50, -1);
        state = STATE_WAITING_FOR_WAKE_WORD;
        break; }
    case STATE_COMPLETED_CMD_ERROR: {
        static int cnt;
        cnt++;
        if (cnt == 1) {
            set_leds(LED_RED, 50, -1);
        } else if (cnt == 8000) {
            set_leds(LED_BLUE, 50, -1);
            state = STATE_WAITING_FOR_WAKE_WORD;
            cnt = 0;
        }
        break; }
    }
        
    // return 0 to continue
    return 0;
}

// -----------------  SET LEDS  --------------------------------------------------

static void convert_angle_to_led_num(double angle, int *led_a, int *led_b);

static void set_leds(unsigned int color, int led_brightness, double doa)
{
    leds_stage_all(color, led_brightness);

    if (doa != -1) {
        int led_a, led_b;
        convert_angle_to_led_num(doa, &led_a, &led_b);
        if (led_a != -1) leds_stage_led(led_a, LED_LIGHT_BLUE, led_brightness);
        if (led_b != -1) leds_stage_led(led_b, LED_LIGHT_BLUE, led_brightness);
    }

    leds_commit();
}

static void convert_angle_to_led_num(double angle, int *led_a, int *led_b)
{
    #define MAX_LEDS 12

    // this ifdef selects between returning one or two led_nums to
    // represent the angle
#if 0
    *led_a = nearbyint( normalize_angle(angle) / (360/MAX_LEDS) );
    if (*led_a == MAX_LEDS) *led_a = 0;
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

