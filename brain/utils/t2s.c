#include <utils.h>

// xxx use #define for sound card
// xxx don't use aplay

static int curr_vol;

void t2s_init(void)
{
    t2s_set_volume(DEFAULT_VOLUME, false);
}

void t2s_play(char *fmt, ...)
{
    int rc;
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

    // use aplay to play the output from synthesize_text
    INFO("RUN_PROG aplay\n");
    rc = system("aplay -D sysdefault:CARD=Device output.raw");
    if (rc < 0) {
        ERROR("system(aplay) failed, rc=%d, %s\n", rc, strerror(errno));
        return;
    }
    INFO("APLAY DONE\n");
}

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

