#include <common.h>

//
// prototypes
//

static void sig_hndlr(int sig);
static int recv_mic_data(const void *frame_arg, void *cx);
static void set_leds(unsigned int color, int brightness, double doa);
static void convert_angle_to_led_num(double angle, int *led_a, int *led_b);

// -----------------  MAIN  ------------------------------------------------------

int main(int argc, char **argv)
{
    int rc;

    // init logging
    logging_init(NULL, false);

    // register for SIGINT and SIGTERM
    static struct sigaction act;
    act.sa_handler = sig_hndlr;
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGTERM, &act, NULL);

    // call init routines
    INFO("INITIALIZING\n")
    misc_init();
    pa_init();
    wwd_init();
    t2s_init();
    s2t_init();
    doa_init();
    leds_init();
    sf_init();
    proc_cmd_init();  // this calls grammar_init

    // set leds blue
    set_leds(LED_BLUE, 50, -1);

    // call portaudio util to start acquiring mic data;
    // - recv_mic_data callback will be called repeatedly with the mic data;
    // - pa_record2 blocks until recv_mic_data returns non-zero
    INFO("RUNNING\n");
    t2s_play("program running");
    rc =  pa_record2("seeed-4mic-voicecard",
                     4,                   // max_chan
                     48000,               // sample_rate
                     PA_INT16,            // 16 bit signed data
                     recv_mic_data,       // callback
                     NULL,                // cx passed to recv_mic_data
                     0);                  // discard_samples count
    if (rc < 0) {
        ERROR("pa_record2\n");
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

// -----------------  XXXXXXXXXXXX  ----------------------------------------------

// called at sample rate 48000
static int recv_mic_data(const void *frame_arg, void *cx)
{
    #define STATE_WAITING_FOR_WAKE_WORD  0
    #define STATE_RECEIVING_CMD          1
    #define STATE_PROCESSING_CMD         2
    #define STATE_DONE_WITH_CMD          3

    const short *frame = frame_arg;

    static int    state = STATE_WAITING_FOR_WAKE_WORD;
    static double doa;

    // check if this program is terminating
    if (end_program) {
        return -1;
    }

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

