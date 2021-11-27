#include <common.h>

//
// defines
//

#define MAX_RECORDING (60*16000)

#define LEDS_IDLE              1
#define LEDS_RECV_AND_PROC_CMD 2
#define LEDS_ERROR             3

//
// variables
//

static bool  end_program;
static short recording[4][MAX_RECORDING];
static int   recording_idx;

//
// prototypes
//

static void initialize(void);
static void sig_hndlr(int sig);
static int proc_mic_data(short *frame);
static void set_leds(int cmd, int doa);
static void *leds_thread(void *cx);

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

    // wait for end_pgm
    while (!end_program) {
        usleep(10*MS);
    }
    
    // program is terminating
    INFO("PROGRAM TERMINATING\n")
    audio_out_cancel();
    t2s_play("program terminating");

    // return success
    return 0;
}

static void initialize(void)
{
    uint64_t secs_since_boot;
    pthread_t tid;

    // register for SIGINT and SIGTERM
    static struct sigaction act;
    act.sa_handler = sig_hndlr;
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGTERM, &act, NULL);

    // workaround problem of audio not working if this program is
    // started too soon after boot
    secs_since_boot = microsec_timer()/SECONDS;
    if (secs_since_boot < 20) {
        INFO("secs_since_boot=%lld, sleeping 10 secs\n", secs_since_boot);
        sleep(10);
    }

    // gpio is used to enable the respeaker leds and to turn body power on/off
    if (wiringPiSetupGpio() != 0) {
        FATAL("wiringPiSetupGpio failed\n");
    }

    // random numbers are used for play music shuffle
    srandom(time(NULL));

    // init access to the database, and read program settings
    db_init("db.dat", true, GB);
    settings.volume = db_get_num(KEYID_PROG_SETTINGS, "volume", 20);
    settings.brightness = db_get_num(KEYID_PROG_SETTINGS, "brightness", 60);
    settings.color_organ = db_get_num(KEYID_PROG_SETTINGS, "color_organ", 2);
    settings.led_scale_factor = db_get_num(KEYID_PROG_SETTINGS, "led_scale_factor", 3.0);

    // init other functions
    misc_init();
    wwd_init();
    t2s_init();
    s2t_init();
    doa_init();
    leds_init(settings.led_scale_factor);
    sf_init();
    proc_cmd_init();
    audio_init(proc_mic_data, settings.volume);
    body_init();

    // create thread to display leds
    pthread_create(&tid, NULL, leds_thread, NULL);
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

void brain_get_recording(short *mic[4], int max)
{
    int ri = recording_idx;

    assert(max < MAX_RECORDING - 1*16000);

    for (int i = 0; i < 4; i++) {
        if (ri-max >= 0) {
            memcpy(mic[i], recording[i]+(ri-max), max*sizeof(short));
        } else {
            int tmp = -(ri-max);
            memcpy(mic[i], recording[i]+(MAX_RECORDING-tmp), tmp*sizeof(short));
            memcpy(mic[i]+tmp, recording[i], (max-tmp)*sizeof(short));
        }
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
    static double filter_cx[4];

    short filtered_frame[4];
    short sound_val;

    // supply the frame for doa analysis, frame is 4 shorts
    doa_feed(frame);

    // discard 2 out of 3 frames, so the sample rate for the code following is 16000
    static int discard_cnt;
    if (++discard_cnt < 3) {
        return 0;
    }
    discard_cnt = 0;

    // filter the 4 microphone channels to remove some of the high pitch background noise
    for (int mic = 0; mic < 4; mic++) {
        int tmp = 4 * low_pass_filter(frame[mic], &filter_cx[mic], 0.90);
        filtered_frame[mic] = clip_int(tmp, -32767, 32767);
    }

    // save sound recording so it can be played to test audio quality
    for (int mic = 0; mic < 4; mic++) {
        recording[mic][recording_idx] = filtered_frame[mic];
    }
    recording_idx = (recording_idx == MAX_RECORDING-1 ? 0 : recording_idx+1);

    // all channels sound about the same; so the code following will always use
    // the sound from microphone channel 0
    sound_val = filtered_frame[0];

    // process mic data state machine
    switch (state) {
    case STATE_WAITING_FOR_WAKE_WORD: {
        if (wwd_feed(sound_val) & WW_KEYWORD_MASK) {
            state = STATE_RECEIVING_CMD;
            doa = doa_get();
            set_leds(LEDS_RECV_AND_PROC_CMD, doa);
        }
        break; }
    case STATE_RECEIVING_CMD: {
        char *transcript = s2t_feed(sound_val);
        if (transcript) {
            if (strcmp(transcript, "TIMEDOUT") == 0) {
                free(transcript);
                set_leds(LEDS_IDLE, -1);
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
        set_leds(LEDS_IDLE, -1);
        state = STATE_WAITING_FOR_WAKE_WORD;
        break; }
    case STATE_COMPLETED_CMD_ERROR: {
        set_leds(LEDS_ERROR, -1);
        state = STATE_WAITING_FOR_WAKE_WORD;
        break; }
    }
        
    // return 0 to continue
    return 0;
}

// -----------------  LEDS THREAD  -----------------------------------------------

static int leds_cmd;
static int leds_doa;
static void convert_angle_to_led_num(double angle, int *led_a, int *led_b);

static void set_leds(int cmd, int doa)
{
    leds_doa = doa; 
    __sync_synchronize();
    leds_cmd = cmd;
}

static void *leds_thread(void *cx)
{
    static bool     rotating = false;
    static int      rotating_cnt = 0;
    static uint64_t set_leds_idle_time = 0;

    set_leds(LEDS_IDLE, -1);

    while (true) {
        switch (leds_cmd) {
        case LEDS_IDLE:
            for (int i = 0; i < MAX_LED; i++) {
                leds_stage_led(i, LED_BLUE, 50 * (i + 4) / MAX_LED);
            }
            leds_commit(settings.brightness);
            rotating = true;
            set_leds_idle_time = 0;
            break;
        case LEDS_RECV_AND_PROC_CMD:
            leds_stage_all(LED_WHITE, 50);
            if (leds_doa != -1) {
                int led_a, led_b;
                convert_angle_to_led_num(leds_doa, &led_a, &led_b);
                if (led_a != -1) leds_stage_led(led_a, LED_LIGHT_BLUE, 80);
                if (led_b != -1) leds_stage_led(led_b, LED_LIGHT_BLUE, 80);
            }
            leds_commit(settings.brightness);
            rotating = false;
            set_leds_idle_time = 0;
            break;
        case LEDS_ERROR:
            leds_stage_all(LED_RED, 50);
            leds_commit(settings.brightness);
            rotating = false;
            set_leds_idle_time = microsec_timer() + 300000;
            break;
        default:
            break;
        }
        leds_cmd = 0;

        if (set_leds_idle_time != 0 && microsec_timer() > set_leds_idle_time) {
            set_leds(LEDS_IDLE, -1);
        } else if (rotating && rotating_cnt++ >= 20) {
            leds_stage_rotate(1);
            leds_commit(settings.brightness);
            rotating_cnt = 0;
        }

        usleep(10*MS);
    }

    return NULL;
}

static void convert_angle_to_led_num(double angle, int *led_a, int *led_b)
{
    // this ifdef selects between returning one or two led_nums to
    // represent the angle
#if 0
    *led_a = nearbyint( normalize_angle(angle) / (360/MAX_LED) );
    if (*led_a == MAX_LED) *led_a = 0;
    *led_b = -1;
#else
    int tmp = nearbyint( normalize_angle(angle) / (360/(2*MAX_LED)) );

    if ((tmp & 1) == 0) {
        *led_a = tmp/2;
        *led_b = -1;
    } else {
        *led_a = tmp/2;
        *led_b = *led_a + 1;
    }

    if (*led_a == MAX_LED) *led_a = 0;
    if (*led_b == MAX_LED) *led_b = 0;
#endif
}
