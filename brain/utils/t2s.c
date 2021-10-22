// xxx add tool to dump the db
#include <utils.h>

static int cnt_db;
static int cnt_synth_text;
static int cnt_synth_text_nodb;

// -----------------  INIT  ------------------------------------------------

void t2s_init(void)
{
    t2s_set_volume(DEFAULT_VOLUME, false);
}

// -----------------  PLAY  ------------------------------------------------

void t2s_play_nodb(char *fmt, ...)
{
    char cmd[10000], *p=cmd;
    va_list ap;

    // make cmd string to run go pgm synthesize_text
    p += sprintf(p, "./go/synthesize_text --text \"");
    va_start(ap, fmt);
    p += vsprintf(p, fmt, ap);
    va_end(ap);
    p += sprintf(p, "\"");

    // run synthesize_text to convert text to wav file, output.raw
    if (system(cmd) < 0) {
        ERROR("system(synthesize_text)) failed, %s\n", strerror(errno));
        return;
    }

    // play
    audio_out_play_wav("output.raw", NULL, 0);

    // print stats
    INFO("t2s_play audio stats: db=%d synth_text=%d synth_text_nodb=%d\n", 
         cnt_db, cnt_synth_text, cnt_synth_text_nodb);
}

void t2s_play(char *fmt, ...)
{
    va_list ap;
    int max_data;
    char cmd[10000], *p=cmd, *text;
    void *val;
    short *data;
    unsigned int val_len;

    // make cmd string to run go pgm synthesize_text
    p += sprintf(p, "./go/synthesize_text --text \"");
    text = p-1;
    va_start(ap, fmt);
    p += vsprintf(p, fmt, ap);
    va_end(ap);
    p += sprintf(p, "\"");

    // if requested text is available in database then 
    //   play the buffer from db to audio_out_play_data
    // else
    //   run synthesize_text to convert text to wav file, output.raw
    //   play the wav file that was provided by the call above to synthesize_text
    //   save synthesize_text result in db
    //   
    if (db_get(KEYID_T2S, text, &val, &val_len) == 0) {
        audio_out_play_data((short*)val, val_len/sizeof(short));
        cnt_db++;
    } else {
        if (system(cmd) < 0) {
            ERROR("system(synthesize_text)) failed, %s\n", strerror(errno));
            return;
        }
        cnt_synth_text++;

        audio_out_play_wav("output.raw", &data, &max_data);

        if (db_set(KEYID_T2S, text, data, max_data*sizeof(short)) < 0) {
            ERROR("db_set %s failed\n", text);
        }
        free(data);
    }

    // print stats
    INFO("t2s_play audio stats: db=%d synth_text=%d synth_text_nodb=%d\n", 
         cnt_db, cnt_synth_text, cnt_synth_text_nodb);
}

// -----------------  VOLUME SUPPORT  --------------------------------------

static int curr_vol;

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

