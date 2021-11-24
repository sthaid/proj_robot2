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

void cleanup_url(char *url);
void cleanup_description(char *description);
void log_msg(char *lvl, char *fmt, ...) __attribute__ ((format (printf, 2, 3)));
void t2s_play(char *fmt, ...) __attribute__((format(printf, 1, 2)));

// -----------------  MAIN  -------------------------------------

int main(int argc, char **argv)
{
    FILE       *fp;
    char       *transcript, *s, *description;
    char        cmd[1000], url[1000], wikipedia_page[1000], filename[1000];
    char        lines[10][10000];
    struct stat statbuf;
    int         rc, n;

    // transcript must be supplied in argv[1]
    if (argc != 2) {
        ERROR("argc = %d\n", argc);
        return 1;
    }

    // print program starting message
    transcript = argv[1];
    INFO("STARTING: TRANSCRIPT = '%s'\n", transcript);

    // perform google customsearch of  www.en.wikipedia.org/*, and
    // extract the best match url from the result
    sprintf(cmd, "./customsearch.py \"%s\" | grep htmlFormattedUrl | awk -F\\' '{ print $4}' | head -1", transcript);
    INFO("CUSTOMSEARCH CMD = '%s'\n", cmd);
    fp = popen(cmd, "r");
    fgets(url, sizeof(url), fp);
    pclose(fp);
    cleanup_url(url);
    INFO("URL = '%s'\n", url);

    // if file scrape/<url> does not exist
    //   run beautifulsoup web scraper on the url
    // endif
    if (sscanf(url, "https://en.wikipedia.org/wiki/%s", wikipedia_page) != 1) {
        ERROR("XXX\n");
    }
    sprintf(filename, "scrape/%s", wikipedia_page);
    INFO("FILENAME = '%s'\n", filename);
    if (stat(filename, &statbuf) < 0) {
        sprintf(cmd, "./beautifulsoup.py %s > %s", url, filename);
        INFO("BEAUTIFYSOUP CMD = '%s'\n", cmd);
        if ((rc = system(cmd)) != 0) {
            ERROR("beautifysoup.py rc=0x%xx\n", rc);
            // xxx returns
        }
    } else {
        INFO("FILE ALREADY EXISTS\n");
    }

    // read the scrape file contents into lines array
    n = 0;
    fp = fopen(filename, "r");  // xxx check
    while (s = lines[n], fgets(s, sizeof(lines[n]), fp) != NULL) {
        if (strlen(s) <= 1) continue;
        if (++n == 10) break;
    }
    fclose(fp);

    // debug print lines
    for (int i = 0; i < n; i++) {
        INFO("LINE[%d] = %s\n", i, lines[i]);
    }

    // play the title
    t2s_play("%s", lines[0]);

    // xxx check this
    if (strlen(lines[1]) < 100 && strlen(lines[2]) > 100) {
        description = lines[2];
    } else {
        description = lines[1];
    }
    cleanup_description(description);
    t2s_play("%s", description);

    // done
    return 0;
}

void cleanup_url(char *url)
{
    char *p;

    // remove trailing newline
    url[strcspn(url, "\n")] = '\0';

    // remove <b> from url
    while (true) {
        p = strstr(url, "<b>");
        if (p == NULL) break;
        memmove(p, p+3, strlen(p+3)+1);
    }

    // remove </b> from url
// xxx optimize
    while (true) {
        p = strstr(url, "</b>");
        if (p == NULL) break;
        memmove(p, p+4, strlen(p+4)+1);
    }
}

void cleanup_description(char *description)
{
    char *p, *end;

    // remove [...]
    p = description;
    while (true) {
        p = strchr(p, '[');
        if (p == NULL) break;
        end = strchr(p, ']');
        if (end == NULL) break;
        memmove(p, end+1, strlen(end+1)+1);
    }

    // remove double quote char
    p = description;
    while (true) {
        p = strchr(p, '"');
        if (p == NULL) break;
        memmove(p, p+1, strlen(p+1)+1);
    }
}

// -----------------  LOGGING  ------------------------------------

// xxx review brain log_msg and t2s_play against this

void log_msg(char *lvl, char *fmt, ...)
{
    char str[10000];
    va_list ap;
    int len;

    va_start(ap, fmt);
    vsnprintf(str, sizeof(str), fmt, ap);
    va_end(ap);

    len = strlen(str);
    str[len-1] = '\n';

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

// xxx this is calling into brain dir
void t2s_play_common(bool nodb, char *fmt, va_list ap)
{
    char  text[10000], cmd[10000];
    int   len;

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
    snprintf(cmd, sizeof(cmd), "../../go/synthesize_text --text \"%s\"", text);
    INFO("T2S CMD: '%s'\n", cmd);
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

