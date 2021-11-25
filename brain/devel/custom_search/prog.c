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

int customsearch(char *transcript);
void readline(char *s, int slen, FILE *fp, bool *eof);
void cleanup_url(char *url);
void cleanup_description(char *description);

void log_msg(char *lvl, char *fmt, ...) __attribute__ ((format (printf, 2, 3)));
void t2s_play(char *fmt, ...) __attribute__((format(printf, 1, 2)));

// -----------------  MAIN  -------------------------------------

int main(int argc, char **argv)
{
    char *transcript;
    int rc;

    // transcript must be supplied in argv[1]
    if (argc != 2) {
        ERROR("argc = %d\n", argc);
        return 1;
    }

    // call customesearch
    transcript = argv[1];
    rc = customsearch(transcript);

    // done 
    return rc < 0 ? 1 : 0;
}

int customsearch(char *transcript)
{
    FILE       *fp;
    char        title[1000], description[10000];
    char        cmd[1000], url[1000], wikipedia_page[1000], filename[1000];
    struct stat statbuf;
    int         rc;
    bool        eof;

    // print search request transcript
    INFO("transcript = '%s'\n", transcript);

    // perform google customsearch of  www.en.wikipedia.org/*, and
    // extract the best match url from the result
    sprintf(cmd, "./customsearch.py \"%s\" | grep htmlFormattedUrl | awk -F\\' '{ print $4}' | head -1", transcript);
    INFO("customsearch cmd = '%s'\n", cmd);
    fp = popen(cmd, "r");
    if (fp == NULL) {
        ERROR("popen customsearch.py failed, %s\n", strerror(errno));
        return -1;
    }
    readline(url, sizeof(url), fp, &eof);
    pclose(fp);
    if (eof) {
        ERROR("failed to read url from customsearch.py\n");
        return -1;
    }
    cleanup_url(url);
    INFO("url = '%s'\n", url);

    // generate scrape filename from url
    if (sscanf(url, "https://en.wikipedia.org/wiki/%s", wikipedia_page) != 1) {
        ERROR("bad url '%s'\n", url);
        return -1;
    }
    sprintf(filename, "scrape/%s", wikipedia_page);
    INFO("filename = '%s'\n", filename);

    // if file scrape file does not exist
    //   run beautifulsoup web scraper on the url
    // endif
    if (stat(filename, &statbuf) < 0) {
        sprintf(cmd, "./beautifulsoup.py %s > %s", url, filename);
        INFO("beautifysoup cmd = '%s'\n", cmd);
        if ((rc = system(cmd)) != 0) {
            ERROR("beautifysoup.py rc=0x%xx\n", rc);
            return -1;
        }
    } else {
        INFO("file %s already exists\n", filename);
    }

    // open the scrape file
    fp = fopen(filename, "r");
    if (fp == NULL) {
        ERROR("failed to open %s, %s\n", filename, strerror(errno));
        return -1;
    }

    // get title from the scrape file
    readline(title, sizeof(title), fp, &eof);

    // get description from the scrape file;
    //
    // sample wikipedia pages have been examined, and the following seems okay:
    // - description is first line following the title whose length is
    //   greater than 100 chars, with the subsequent line appended
    while (true) {
        readline(description, sizeof(description), fp, &eof);
        if (eof) break;

        int len = strlen(description);
        if (len > 100) {
            readline(description+len, sizeof(description)-len, fp, &eof);
            break;
        }
    }
    if (description[0] == '\0') {
        strcpy(description, "no information available");
    }

    // close scrape file
    fclose(fp);

    // play the title and description
    t2s_play("%s", title);

    cleanup_description(description);
    INFO("description len=%zd - '%s'\n", strlen(description), description);
    t2s_play("%s", description);

    // done
    return 0;
}

void readline(char *s, int slen, FILE *fp, bool *eof)
{
    if (fgets(s, slen, fp) == NULL) {
        s[0] = '\0';
        *eof = true;
        return;
    }

    s[strcspn(s, "\n")] = '\0';
    *eof = false;
}

void cleanup_url(char *url)
{
    char *p;

    // remove <b> from url
    while (true) {
        p = strstr(url, "<b>");
        if (p == NULL) break;
        memmove(p, p+3, strlen(p+3)+1);
    }

    // remove </b> from url
    while (true) {
        p = strstr(url, "</b>");
        if (p == NULL) break;
        memmove(p, p+4, strlen(p+4)+1);
    }
}

void cleanup_description(char *description)
{
    char *p, *end, *start;
    int level;

    // remove double quote char
    p = description;
    while (true) {
        p = strchr(p, '"');
        if (p == NULL) break;
        memmove(p, p+1, strlen(p+1)+1);
    }

    // remove [...]
    p = description;
    while (true) {
        p = strchr(p, '[');
        if (p == NULL) break;
        end = strchr(p, ']');
        if (end == NULL) break;
        memmove(p, end+1, strlen(end+1)+1);
    }

    // remove (...(...)...)
    p = description;
    level = 0;
    start = NULL;
    while (*p != '\0') {
        if (*p == '(') {
            if (level == 0) start = p;
            level++;
        }
        if (*p == ')') {
            level--;
            if ((level < 0) || (level == 0 && start == NULL)) {
                break;
            }
            if (level == 0) {
#if 0
                // debug print what is being removed
                char tmp[10000];
                memcpy(tmp, start, p-start+1);
                tmp[p-start+1] = '\0';
                INFO("removing '%s'\n", tmp);
#endif

                memmove(start, p+1, strlen(p+1)+1);
                p = start-1;
                start = NULL;
            }
        }
        p++;
    }

    // if descriptin len is > 500 then shorten it by starting at 500 and
    // null terminating at the first period found
    if (strlen(description) > 500) {
        for (p = description+500; *p; p++) {
            if (*p == '.' && *(p+1) == ' ') {
                *(p+1) = '\0';
                break;
            }
        }
    }
}

// -----------------  LOGGING  ------------------------------------

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

    // conver text to sound file
    snprintf(cmd, sizeof(cmd), "../text_to_speech/synthesize_text --text \"%s\"", text);
    INFO("t2s cmd: '%s'\n", cmd);

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
