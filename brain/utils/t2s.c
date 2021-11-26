#include <utils.h>

// prototypes
static void t2s_play_common(bool nocache, char *fmt, va_list ap);

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

void t2s_play_nocache(char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    t2s_play_common(true, fmt, ap);
    va_end(ap);
}

static void t2s_play_common(bool nocache, char *fmt, va_list ap)
{
    char text[10000], cmd[11000];
    int  text_len;
    unsigned int crc;
    char pathname[100];
    struct stat statbuf;

    // sprint the caller's fmt/ap to text, and sprint the cmd to run synthesize_text;
    vsnprintf(text, sizeof(text), fmt, ap);

    // if newline char is present in text then remove it;
    text_len = strlen(text);
    if (text_len > 0 && text[text_len-1] == '\n') {
        text[text_len-1] = '\0';
        text_len--;
    }

    // if lenght of text is 0 then return
    if (text_len == 0) {
        return;
    }

    // debug print the text
    INFO("PLAY: %s\n", text);

    // if caller requests to not use the speech cache
    //   create file tmp.wav using synthesize_text
    //   call audio_out_play_wav
    //   return
    // endif
    if (nocache) {
        sprintf(cmd, "./go/synthesize_text --text \"%s\" --output-file tmp.wav", text);
        if (system(cmd) < 0) {
            ERROR("system(synthesize_text)) failed, %s\n", strerror(errno));
            return;
        }
        audio_out_play_wav("tmp.wav", true);
        return;
    }

    // create sound cache wav pathname by combining the crc and length of the text
    crc = crc32(text, text_len);
    sprintf(pathname, "speech_cache/%08x_%04x.wav", crc, text_len);

    // if the file doesnt exists then
    //   create the file using synthesize_text
    // endif
    // call audio_out_play_wav
    if (stat(pathname, &statbuf) < 0) {
        sprintf(cmd, "./go/synthesize_text --text \"%s\" --output-file %s", text, pathname);
        if (system(cmd) < 0) {
            ERROR("system(synthesize_text)) failed, %s\n", strerror(errno));
            return;
        }
    }
    audio_out_play_wav(pathname, true);
}
