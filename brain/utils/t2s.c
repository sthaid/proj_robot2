#include <utils.h>

// xxx use #define for sound card

//
// defines
//

#define BEEP_DURATION_MS 200
#define BEEP_FREQUENCY   800
#define BEEP_AMPLITUDE   6000
#define MAX_BEEP_DATA    (48000 * BEEP_DURATION_MS / 1000)

//
// variables
//

static int curr_vol;

static struct {
    short *data;
    int    max_data;
    int    idx;
} play;

static struct {
    short data[MAX_BEEP_DATA];
    int   idx;
    int   n;
} beep;

//
// prototypes
//

static int get_play_frame(void *data_arg, void *cx);
static int get_beep_frame(void *data_arg, void *cx);

// -----------------  INIT  ------------------------------------------------

void t2s_init(void)
{
    int i;

    t2s_set_volume(DEFAULT_VOLUME, false);

    for (i = MAX_BEEP_DATA/2; i < MAX_BEEP_DATA; i++) {
        beep.data[i] = BEEP_AMPLITUDE * sin(i * (2*M_PI / MAX_BEEP_DATA * BEEP_FREQUENCY));
    }
}

// -----------------  PLAY  ------------------------------------------------

void t2s_play(char *fmt, ...)
{
    int rc, max_chan, sample_rate;
    char cmd[10000], *p=cmd;
    va_list ap;

    // make cmd string to run go pgm synthesize_text
    p += sprintf(p, "./go/synthesize_text --text \"");
    va_start(ap, fmt);
    p += vsprintf(p, fmt, ap);
    va_end(ap);
    p += sprintf(p, "\"");

    // run synthesize_text to convert text to wav file;
    INFO("RUN_PROG '%s'\n", cmd);
    rc = system(cmd);
    if (rc < 0) {
        ERROR("system(synthesize_text)) failed, rc=%d, %s\n", rc, strerror(errno));
        return;
    }
    INFO("RUN_PROG done\n");

    // read wav file 'output.raw', that was just created by synthesize_text
    memset(&play, 0, sizeof(play));
    rc = sf_read_wav_file("output.raw", &play.data, &max_chan, &play.max_data, &sample_rate);
    if (rc < 0) {
        ERROR("sf_read_wav_file failed\n");
        return;
    }
    INFO("max_data=%d  max_chan=%d  sample_rate=%d\n", play.max_data, max_chan, sample_rate);
    assert(play.data != NULL);
    assert(play.max_data > 0);
    assert(max_chan == 1);
    assert(sample_rate == 24000);

    // call play2 to play the contents of play.data to the USB speaker
    pa_play2("USB", 2, 48000, PA_INT16, get_play_frame, NULL);

    // free data
    free(play.data);
}

static int get_play_frame(void *data_arg, void *cx)
{
    short *data = data_arg;

    // xxx can run this all at 24000 by using PA_ALSA_PLUGHW=1
    if (play.idx >= 2*play.max_data)  {
        return -1;
    }

    data[0] = play.data[play.idx/2];
    data[1] = play.data[play.idx/2];
    play.idx++;

    return 0;
}

// -----------------  BEEP  ------------------------------------------------

void t2s_beep(int n)
{
    beep.n = n;
    beep.idx = 0;
    pa_play2("USB", 2, 48000, PA_INT16, get_beep_frame, NULL);
}

static int get_beep_frame(void *data_arg, void *cx)
{
    short *data = data_arg;

    if (beep.n == 0) {
        return -1;
    }

    data[0] = beep.data[beep.idx];
    data[1] = beep.data[beep.idx];

    beep.idx++;
    if (beep.idx >= MAX_BEEP_DATA) {
        beep.idx = 0;
        beep.n--;
    }

    return 0;
}

// -----------------  VOLUME SUPPORT  --------------------------------------

void t2s_set_volume(int percent, bool relative)
{
    char cmd[100];

    if (!relative) {
        curr_vol = percent;
    } else {
        curr_vol += percent;
    }
    if (curr_vol < 0) curr_vol = 0;
    if (curr_vol > 100) curr_vol = 100;

    sprintf(cmd, "amixer -c 2 set PCM Playback Volume %d%%", curr_vol);
    system(cmd);
}

int t2s_get_volume(void)
{
    return curr_vol;
}

