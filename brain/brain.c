#include <common.h>

//
// variables
//

//
// prototypes
//

static void sig_hndlr(int sig);

static void * process_mic_data_thread(void *cx);
static int process_mic_data(short *frame);

static void set_leds(unsigned int color, int brightness, double doa);
static void convert_angle_to_led_num(double angle, int *led_a, int *led_b);

// -----------------  MAIN  ------------------------------------------------------

int main(int argc, char **argv)
{
    pthread_t tid;

    // register for SIGINT and SIGTERM
    static struct sigaction act;
    act.sa_handler = sig_hndlr;
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGTERM, &act, NULL);

    // init logging
    log_init(NULL, false, false);

    // call init routines
    // xxx db_init madvise
    // xxx audio_init madvise
    INFO("INITIALIZING\n")
    misc_init();
    wwd_init();
    t2s_init();
    s2t_init();
    doa_init();
    leds_init();
    sf_init();
    proc_cmd_init();
    audio_init();

    // xxx remember madvise,   for both mmap regions

    // xxx this should be handled in audio_init
    // create thread to process the mic data
    // xxx create and join in the audio routines
    pthread_create(&tid, NULL, process_mic_data_thread, NULL);

    // program is running
    INFO("RUNNING\n");
    set_leds(LED_BLUE, 50, -1);
    t2s_play("program running");

    // wait for end_pgm
    // xxx t2s_play 
    while (!end_program) {
        usleep(100000);
    }
    
    // program is terminating
    INFO("TERMINATING\n")
    t2s_play("program terminating");
    set_leds(LED_OFF, 0, -1);
    return 0;
}

static void sig_hndlr(int sig)
{
    end_program = true;
}

// xxx add end_prgoram api

// -----------------  XXXXXXXXXXXX  ----------------------------------------------

// xxx move to audio.c
static void * process_mic_data_thread(void *cx)  // xxx join
{
    int curr_fidx;
    int last_fidx = 0;

    while (true) {
        curr_fidx = shm->fidx;
        while (last_fidx != curr_fidx) {
            process_mic_data(shm->frames[last_fidx]);
            last_fidx = (last_fidx + 1) % 48000;
        }

        usleep(1000);
    }

    return NULL;
}

static int process_mic_data(short *frame)
{
    #define STATE_WAITING_FOR_WAKE_WORD  0
    #define STATE_RECEIVING_CMD          1
    #define STATE_PROCESSING_CMD         2
    #define STATE_DONE_WITH_CMD          3

    static int    state = STATE_WAITING_FOR_WAKE_WORD;
    static double doa;

    // sample rate is 48000

    // supply the frame for doa analysis, frame is 4 shorts
    doa_feed(frame);

    // discard 2 out of 3 frames, so the sample rate for the code following is 16000
    static int discard_cnt;
    if (++discard_cnt < 3) {
        return 0;
    }
    discard_cnt = 0;

    // the sound value used for the remaining of this routine is the value from mic chan 0
    short sound_val = frame[0];

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
                state = STATE_DONE_WITH_CMD;
                break;
            }
            proc_cmd_execute(transcript, doa);
            state = STATE_PROCESSING_CMD;
        }
        break; }
    case STATE_PROCESSING_CMD: {
        if (proc_cmd_in_progress() == false) {
            state = STATE_DONE_WITH_CMD;
            break;
        }
        if (wwd_feed(sound_val) & WW_TERMINATE_MASK) {
            INFO("CANCELLING\n");
            proc_cmd_cancel();
        }
        break; }
    case STATE_DONE_WITH_CMD: {
        doa = -1;
        set_leds(LED_BLUE, 50, -1);
        state = STATE_WAITING_FOR_WAKE_WORD;
        break; }
    }
        
    // return 0 to continue
    return 0;
}

// -----------------  XXXXXXXXXXXX  ----------------------------------------------

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

