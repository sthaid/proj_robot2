#include <common.h>

static void t2s_play_text(char *text_arg)
{
    int rc;
    char cmd[10000];
    char text[10000];

    // xxx add extra 'the'

    // remove newline char in text_arg
    strcpy(text, text_arg);
    text[strcspn(text,"\n")] = '\0';

    // run synthesize_text to convert text to wav file;
    printf("RUN_PROG synthesize_text, '%s'\n", text);
    sprintf(cmd, "./synthesize_text --text \"%s\"", text);
    rc = system(cmd);
    if (rc < 0) {
        printf("ERROR: system(synthesize_text)) failed, rc=%d, %s\n", rc, strerror(errno));
        return;
    }
    printf("RUN_PROG done\n");

    // use aplay to play the output from synthesize_text
    rc = system("aplay -D sysdefault:CARD=Device output.raw");
    if (rc < 0) {
        printf("ERROR: system(aplay) failed, rc=%d, %s\n", rc, strerror(errno));
        return;
    }
    printf("APLAY DONE\n");
}

