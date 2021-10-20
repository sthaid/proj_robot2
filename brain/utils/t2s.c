#include <utils.h>

//
// defines
//

//
// variables
//

static int curr_vol;

//
// prototypes
//

// -----------------  INIT  ------------------------------------------------

void t2s_init(void)
{
    t2s_set_volume(DEFAULT_VOLUME, false);
}

// -----------------  PLAY  ------------------------------------------------

void t2s_play(char *fmt, ...)
{
    int rc, max_chan, max_data, sample_rate;
    char cmd[10000], *p=cmd;
    short *data;
    va_list ap;

    // make cmd string to run go pgm synthesize_text
    p += sprintf(p, "./go/synthesize_text --text \"");
    va_start(ap, fmt);
    p += vsprintf(p, fmt, ap);
    va_end(ap);
    p += sprintf(p, "\"");

    // run synthesize_text to convert text to wav file, output.raw
    INFO("RUN_PROG '%s'\n", cmd);
    rc = system(cmd);
    if (rc < 0) {
        ERROR("system(synthesize_text)) failed, rc=%d, %s\n", rc, strerror(errno));
        return;
    }
    INFO("RUN_PROG done\n");

    // read wav file 'output.raw', that was just created by synthesize_text
    // xxx read this directly to shm, or just provide filename to audio.c
    rc = sf_read_wav_file("output.raw", &data, &max_chan, &max_data, &sample_rate);
    if (rc < 0) {
        ERROR("sf_read_wav_file failed\n");
        return;
    }
    INFO("max_data=%d  max_chan=%d  sample_rate=%d\n", max_data, max_chan, sample_rate);
    assert(data != NULL);
    assert(max_data > 0);
    assert(max_chan == 1);
    assert(sample_rate == 24000);

    // play
    audio_out_play(data, max_data);

    // free data
    free(data);
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

