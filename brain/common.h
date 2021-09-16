#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <math.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include <utils.h>

// -----------------  LOGGING  ------------------------------------

#define MAX_VERBOSE 4
bool verbose[MAX_VERBOSE];
FILE *fp_log;

#define PRINT_COMMON(lvl, fmt, args...) \
    do { \
        char _s[100]; \
        fprintf(fp_log, "%s " lvl ": " fmt, time2str(time(NULL),_s), ## args); \
    } while (0)

#define INFO(fmt, args...) PRINT_COMMON("INFO", fmt, ## args);
#define WARN(fmt, args...) PRINT_COMMON("WARN", fmt, ## args);
#define ERROR(fmt, args...) PRINT_COMMON("ERROR", fmt, ## args);

#define VERBOSE0(fmt, args...) do { if (verbose[0]) PRINT_COMMON("VERBOSE0", fmt, ## args); } while (0)
#define VERBOSE1(fmt, args...) do { if (verbose[1]) PRINT_COMMON("VERBOSE1", fmt, ## args); } while (0)
#define VERBOSE2(fmt, args...) do { if (verbose[2]) PRINT_COMMON("VERBOSE2", fmt, ## args); } while (0)
#define VERBOSE3(fmt, args...) do { if (verbose[3]) PRINT_COMMON("VERBOSE3", fmt, ## args); } while (0)

#define FATAL(fmt, args...) do { PRINT_COMMON("FATAL", fmt, ## args); exit(1); } while (0)

// -----------------  XXXXXXXXXXXX  -------------------------------

void wwd_init(void);
void t2s_init(void);
void s2t_init(void);
void leds_init(void);
void doa_init(void);
void pa_init(void);

uint64_t microsec_timer(void);
char *time2str(time_t t, char *s);
// xxx run_program


