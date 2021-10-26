#include <utils.h>
  
static bool log_brief;
static FILE *log_fp;

void log_init(char *filename, bool append, bool brief)
{
    log_brief = brief;

    if (filename == NULL) {
        log_fp = stdout;
    } else {
        log_fp = fopen(filename, append ? "a" : "w");
        if (log_fp == NULL) {
            printf("FATAL: failed to open log file %s, %s\n", filename, strerror(errno));
            exit(1);
        }
    }

    setlinebuf(log_fp);
}

void log_msg(char *lvl, char *fmt, ...)
{
    char str[1000], s[100];
    va_list ap;

    if (log_fp == NULL) {
        printf("ERROR: log_fp is not set\n");
        exit(1);
    }

    va_start(ap, fmt);
    vsprintf(str, fmt, ap);
    va_end(ap);

    if (!log_brief) { 
        fprintf(log_fp, "%s %s: %s", time2str(time(NULL),s), lvl, str);
    } else if (strcmp(lvl, "INFO") != 0) { 
        fprintf(log_fp, "%s: %s", lvl, str);
    } else { 
        fprintf(log_fp, "%s", str);
    } 
}
