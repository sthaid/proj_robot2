#include <utils.h>

// defines
#define KEYID_T2S 1   // also defined in common.h

// variables
static int cnt_db;
static int cnt_synth_text;
static int cnt_synth_text_nodb;

// prototypes
static void t2s_play_common(bool nodb, char *fmt, va_list ap);

// -----------------  INIT  ------------------------------------------------

void t2s_init(void)
{
    // nothing
}

// -----------------  PLAY  ------------------------------------------------

void t2s_play(char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    t2s_play_common(false, fmt, ap);
    va_end(ap);
}

void t2s_play_nodb(char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    t2s_play_common(true, fmt, ap);
    va_end(ap);
}

static void t2s_play_common(bool nodb, char *fmt, va_list ap)
{
    char         text[1000], cmd[1100];
    void        *val;
    unsigned int val_len;
    short       *data;
    int          max_data, len;

    // sprint the caller's fmt/ap to text, and sprint the cmd to run synthesize_text;
    // if newline char is present in text then remove it;
    // if lenght of text is 0 then return
    vsprintf(text, fmt, ap);

    len = strlen(text);
    if (len > 0 && text[len-1] == '\n') {
        text[len-1] = '\0';
        len--;
    }

    if (len == 0) {
        return;
    }

    // debug print the text
    INFO("PLAY: %s\n", text);

    // if caller requests that the databse not be used then
    //   run synthesize_text to convert text to wav file, output.raw
    //   play the wav file that was provided by the call above to synthesize_text
    // else if requested text is available in database then 
    //   play the buffer from db to audio_out_play_data
    // else
    //   run synthesize_text to convert text to wav file, output.raw
    //   play the wav file that was provided by the call above to synthesize_text
    //   save synthesize_text result in db
    // endif
    sprintf(cmd, "./go/synthesize_text --text \"%s\"", text);
    if (nodb) {
        if (system(cmd) < 0) {
            ERROR("system(synthesize_text)) failed, %s\n", strerror(errno));
            return;
        }
        cnt_synth_text_nodb++;
        audio_out_play_wav("output.raw", NULL, 0);
    } else {
        if (db_get(KEYID_T2S, text, &val, &val_len) == 0) {
            audio_out_play_data((short*)val, val_len/sizeof(short), 24000);
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
    }

    // print stats
    INFO("t2s_play audio stats: db=%d synth_text=%d synth_text_nodb=%d\n", 
         cnt_db, cnt_synth_text, cnt_synth_text_nodb);
}
