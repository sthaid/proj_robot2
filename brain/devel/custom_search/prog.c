#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>

#define INFO(fmt, args...) log_msg("INFO", fmt, ## args);
#define WARN(fmt, args...) log_msg("WARN", fmt, ## args);
#define ERROR(fmt, args...) log_msg("ERROR", fmt, ## args);

void log_msg(char *lvl, char *fmt, ...) __attribute__ ((format (printf, 2, 3)));
void t2s_play(char *fmt, ...) __attribute__((format(printf, 1, 2)));

// --------------------------------------------------------------

int main(int argc, char **argv)
{
    FILE *fp;
    char url[1000], wikipedia_page[1000], filename[1100];
    struct stat statbuf;
        char cmd[2200];
        int rc;

    printf("argc=%d argv[1]='%s'\n", argc, argv[1]);

    char *transcript = argv[1];

    // perform google customsearch of  www.en.wikipedia.org/*, and
    // extract the best match url from the result
    sprintf(cmd, "./customsearch.py \"%s\" | grep htmlFormattedUrl | awk -F\\' '{ print $4}' | head -1", transcript);
    printf("CMD '%s'\n", cmd);
    fp = popen(cmd, "r");
    fgets(url, sizeof(url), fp);
    fclose(fp);

    // remove <b>
    while (true) {
        char *p = strstr(url, "<b>");
        if (p == NULL) break;
        memmove(p, p+3, strlen(p+3)+1);
        p = strstr(url, "</b>");
        if (p == NULL) break;
        memmove(p, p+4, strlen(p+4)+1);
    }

    url[strcspn(url, "\n")] = '\0';
    printf("'%s'\n", url);

    // if file scrape/<url> does not exist
    //   run beautifulsoup web scraper on the url
    // endif
    sscanf(url, "https://en.wikipedia.org/wiki/%s", wikipedia_page);
    sprintf(filename, "scrape/%s", wikipedia_page);
    printf("filename='%s'\n", filename);
    if (stat(filename, &statbuf) < 0) {
        sprintf(cmd, "./beautifulsoup.py %s > %s", url, filename);
        printf("CMD '%s'\n", cmd);
        rc = system(cmd);
        printf("rc = 0x%x\n", rc);
    }

    // read the scrape file, and extract the Title and Intro paragraph
    char title[1000];
    fp = fopen(filename, "r");  // xxx check

    fgets(title, sizeof(title), fp);
    title[strcspn(title, "\n")] = '\0';
    if (strncmp(title, "Title: ", 7) != 0) {
        printf("ERROR bad title\n");
    }
    printf("'%s'\n", title);

    char paragraph[10][10000];  // xxx 10000
    char s[10000];
    int n = 0;
    // preset xxx to 0 len
    while (true) {
        if (fgets(s, sizeof(s), fp) == NULL) break;
        if (strlen(s) <= 1) continue;
        if (strncmp(s, "Paragraph: ", 11) != 0) {
            printf("ERROR, bad Paragraph '%s'\n", s);
            continue;
        }
        strcpy(paragraph[n], s);
        printf("PARA %d = '%s'\n", n, paragraph[n]);
        n++;
        if (n == 10) break;
    }
    INFO("got %d para\n", n);

    // play the title 
    t2s_play("%s", title+7);

    // play the intro paragraph
    // xxx limit to 500
    paragraph[0][500] = 0;
    paragraph[1][500] = 0;
    t2s_play("%s", paragraph[1]+11);

    // done
    return 0;
}

// -----------------  LOGGING  ------------------------------------

void log_msg(char *lvl, char *fmt, ...)
{
    char str[1000];
    va_list ap;

    va_start(ap, fmt);
    vsprintf(str, fmt, ap);
    va_end(ap);

    fprintf(stdout, "%s: %s", lvl, str);
}

// -----------------  TEXT TO SPEECH  -----------------------------

void t2s_play_common(bool nodb, char *fmt, va_list ap);

void t2s_play(char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    t2s_play_common(false, fmt, ap);
    va_end(ap);
}


void t2s_play_common(bool nodb, char *fmt, va_list ap)
{
    char         text[1000], cmd[1100];
    //void        *val;
    //unsigned int val_len;
    //short       *data;
    //int          max_data, len;
    int len;

    // sprint the caller's fmt/ap to text, and sprint the cmd to run synthesize_text;
    // if newline char is present in text then remove it;
    vsprintf(text, fmt, ap);
    len = strlen(text);
    if (len > 0 && text[len-1] == '\n') {
        text[len-1] = '\0';
        len--;
    }

    // if lenght of text is 0 then return
    if (len == 0) return;

    // debug print text
    INFO("PLAY: %s\n", text);

    // conver text to sound file
    sprintf(cmd, "../../go/synthesize_text --text \"%s\"", text);
    if (system(cmd) < 0) {
        ERROR("system(synthesize_text)) failed, %s\n", strerror(errno));
        return;
    }

    // play the sound file
    if (system("aplay output.raw") < 0) {
        ERROR("system(aplay) failed, %s\n", strerror(errno));
        return;
    }
}

