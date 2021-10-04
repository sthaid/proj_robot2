#include <common.h>

// variables
static bool prog_terminating;

// prototypes
static void sig_hndlr(int sig);
static int recv_mic_data(const void *frame_arg, void *cx);
static void set_leds(unsigned int color, int brightness);

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
    proc_cmd_init();  // this calls grammar_init

    // set leds blue
    set_leds(LED_BLUE, 50);

    // call portaudio util to start acquiring mic data;
    // - recv_mic_data callback will be called repeatedly with the mic data;
    // - pa_record2 blocks until recv_mic_data returns non-zero
    INFO("RUNNING\n");
    rc =  pa_record2("seeed-4mic-voicecard",
                     4,                   // max_chan
                     48000,               // sample_rate
                     PA_FLOAT32,          // 32 bit floating point data
                     recv_mic_data,       // callback
                     NULL,                // cx passed to recv_mic_data
                     0);                  // discard_samples count
    if (rc < 0) {
        ERROR("pa_record2\n");
    }

    // program is terminating
    INFO("TERMINATING\n")
    set_leds(LED_OFF, 0);
    return 0;
}

static void sig_hndlr(int sig)
{
    prog_terminating = true;
}

// -----------------  XXXXXXXXXXXX  ----------------------------------------------

// called at sample rate 48000
static int recv_mic_data(const void *frame_arg, void *cx)
{
    #define STATE_WAITING_FOR_WAKE_WORD  0
    #define STATE_RECEIVING_CMD          1
    #define STATE_PROCESSING_CMD         2
    #define STATE_DONE_WITH_CMD          3

    static int   state = STATE_WAITING_FOR_WAKE_WORD;
    const float *frame = frame_arg;

    // check if this program is terminating
    if (prog_terminating) {
        return -1;
    }

    // supply the frame for doa analysis, frame is 4 float values
    doa_feed(frame);

    // discard 2 out of 3 frames, so the sample rate for the code following is 16000
    static int discard_cnt;
    if (++discard_cnt < 3) {
        return 0;
    }
    discard_cnt = 0;

    // the sound value used for the remaining of this routine is 
    // a single 16 bit signed integer, from mic channel0
    short sound_val = frame[0] * 32767;

    switch (state) {
    case STATE_WAITING_FOR_WAKE_WORD: {
        if (wwd_feed(sound_val) == WW_KEYWORD) {
            state = STATE_RECEIVING_CMD;
            // XXX get doa
            set_leds(LED_WHITE, 100);
        }
        break; }
    case STATE_RECEIVING_CMD: {
        char *transcript = s2t_feed(sound_val);
        if (transcript) {
            if (strcmp(transcript, "TIMEDOUT") == 0) {
                INFO("XXX transcript timedout\n");
                free(transcript);
                state = STATE_DONE_WITH_CMD;
                break;
            }
            proc_cmd_execute(transcript);
            state = STATE_PROCESSING_CMD;
        }
        break; }
    case STATE_PROCESSING_CMD: {
        if (proc_cmd_in_progress() == false) {
            state = STATE_DONE_WITH_CMD;
            break;
        }
        if (wwd_feed(sound_val) == WW_TERMINATE) {
            INFO("CANCELLING\n");
            proc_cmd_cancel();
        }
        break; }
    case STATE_DONE_WITH_CMD: { // xxx is this state needed
        set_leds(LED_BLUE, 50);
        state = STATE_WAITING_FOR_WAKE_WORD;
        break; }
    }
        
    // return 0 to continue
    return 0;
}

// -----------------  XXXXXXXXXXXX  ----------------------------------------------

static void set_leds(unsigned int color, int brightness)
{
    leds_set_all(color, brightness);
    leds_show(color == LED_OFF ? 0 : 31);
}
