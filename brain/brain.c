#include <common.h>

// variables
static bool prog_terminating;

// prototypes
static void sig_hndlr(int sig);
static int recv_mic_data(const void *frame_arg, void *cx);

// -----------------  MAIN  ------------------------------------------------------

int main(int argc, char **argv)
{
    int rc;

#if 0
    // open logfile in append mode
    fp_log = fopen("brain.log", "a");
    if (fp_log == NULL) {
        printf("FATAL: failed to open simple.log, %s\n", strerror(errno));
        exit(1);
    }
    setlinebuf(fp_log);
#else
    // use stdout for logfile
    fp_log = stdout;
    setlinebuf(fp_log);
#endif

    INFO("INITIALIZING\n")

    // register for SIGINT and SIGTERM
    static struct sigaction act;
    act.sa_handler = sig_hndlr;
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGTERM, &act, NULL);

#if 1
    // initialize utils
    pa_init();
    wwd_init();
    t2s_init();
    s2t_init();
    doa_init();
    leds_init();
#endif

    // xxx
    proc_cmd_init();

    //t2s_play_text("program running");

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

    // turn off leds
    leds_set_all_off();
    leds_show(0);

    // xxx
    proc_cmd_exit();
    // xxx others too

    // terminate
    //t2s_play_text("program terminating");
    INFO("TERMINATING\n")
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

    const float *frame = frame_arg;
    static int discard_cnt;
    short sound_val;
    char *transcript;

    static int state = STATE_WAITING_FOR_WAKE_WORD;
    static bool first_call = true;

    // check if this program is terminating
    if (prog_terminating) {
        return -1;
    }

    // xxx
    if (first_call) {
        first_call = false;
        leds_set_all(LED_BLUE, 50);
        leds_show(31);
    }

    // supply the frame for doa analysis, frame is 4 float values
    doa_feed(frame);

    // discard 2 out of 3 frames, so the sample rate for the code following is 16000
    if (++discard_cnt < 3) {
        return 0;
    }
    discard_cnt = 0;
    sound_val = frame[0] * 32767;

    switch (state) {
    case STATE_WAITING_FOR_WAKE_WORD: {
        if (wwd_feed(sound_val) == WW_PORCUPINE) {
            state = STATE_RECEIVING_CMD;
            // get doa
            leds_set_all(LED_WHITE,100);  // xxx tbd, how long does this take, and incorporate doa
            leds_show(31);
        }
        break; }
    case STATE_RECEIVING_CMD: {
        transcript = s2t_feed(sound_val);
        // xxx TIMEDOUT
        if (transcript) {
            proc_cmd_execute(transcript);
            state = STATE_PROCESSING_CMD;
        }
        break; }
    case STATE_PROCESSING_CMD: {
        if (proc_cmd_in_progress() == false) {
            state = STATE_DONE_WITH_CMD;
        }
        if (wwd_feed(sound_val) == WW_TERMINATOR) {
            INFO("CANCELLING\n");
            proc_cmd_cancel();
        }
        break; }
    case STATE_DONE_WITH_CMD: {
        leds_set_all(LED_BLUE, 50);
        leds_show(31);
        state = STATE_WAITING_FOR_WAKE_WORD;
        break; }
    }
        
    // return status to continue
    return 0;
}
