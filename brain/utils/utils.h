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
#include <pthread.h>
#include <signal.h>
#include <assert.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/ioctl.h>


// XXX re-order
#define MAX_VERBOSE 4
bool verbose[MAX_VERBOSE];
FILE *fp_log;

static inline void logging_init(char *filename, bool append)
{
    if (filename == NULL) {
        fp_log = stdout;
    } else {
        fp_log = fopen(filename, append ? "a" : "w");
        if (fp_log == NULL) {
            printf("FATAL: failed to open log file %s, %s\n", filename, strerror(errno));
            exit(1);
        }
        setlinebuf(fp_log);
    }
}

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

// -------- misc.c --------

void misc_init(void);

uint64_t microsec_timer(void);
char *time2str(time_t t, char *s);

unsigned int wavelen_to_rgb(double wavelength);

void run_program(pid_t *prog_pid, int *fd_to_prog, int *fd_from_prog, char *prog, ...);

void poly_fit(int max_data, double *x_data, double *y_data, int degree_of_poly, double *coefficients);

double normalize_angle(double angle);
double max_doubles(double *x, int n, int *max_idx);
double min_doubles(double *x, int n, int *min_idx);
char *stars(double v, double max_v, int max_stars, char *s);

// -------- pa.c --------

#define DEFAULT_OUTPUT_DEVICE "DEFAULT_OUTPUT_DEVICE"
#define DEFAULT_INPUT_DEVICE  "DEFAULT_INPUT_DEVICE"

#define PA_FLOAT32       (0x00000001) // these must match portaudio defines paFloat32, etc
#define PA_INT32         (0x00000002) 
#define PA_INT24         (0x00000004) 
#define PA_INT16         (0x00000008) 
#define PA_INT8          (0x00000010) 
#define PA_UINT8         (0x00000020)

typedef int (*play2_get_frame_t)(void *data, void *cx);
typedef int (*record2_put_frame_t)(const void *data, void *cx);

void pa_init(void);

int pa_play(char *output_device, int max_chan, int max_data, int sample_rate, int sample_format, void *data);
int pa_play2(char *output_device, int max_chan, int sample_rate, int sample_format, play2_get_frame_t get_frame, void *get_frame_cx);

int pa_record(char *input_device, int max_chan, int max_data, int sample_rate, int sample_format, void *data, int discard_samples);
int pa_record2(char *input_device, int max_chan, int sample_rate, int sample_format, record2_put_frame_t put_frame, void *put_frame_cx, int discard_samples);

int pa_find_device(char *name);
void pa_print_device_info(int idx);
void pa_print_device_info_all(void);

// -------- leds.c --------

// notes:
// - led_brightness range  0 - 100
// - all_brightness range  0 - 31
// - xxx rotate mode

#define LED_RGB(r,g,b) ((unsigned int)(((r) << 0) | ((g) << 8) | ((b) << 16)))

#define LED_WHITE      LED_RGB(255,255,255)
#define LED_RED        LED_RGB(255,0,0)
#define LED_PINK       LED_RGB(255,105,180)
#define LED_ORANGE     LED_RGB(255,128,0)
#define LED_YELLOW     LED_RGB(255,255,0)
#define LED_GREEN      LED_RGB(0,255,0)
#define LED_BLUE       LED_RGB(0,0,255)
#define LED_LIGHT_BLUE LED_RGB(0,255,255)
#define LED_PURPLE     LED_RGB(127,0,255)
#define LED_OFF        LED_RGB(0,0,0)

void leds_init(void);

void leds_set(int num, unsigned int rgb, int led_brightness);
void leds_set_all(unsigned int rgb, int led_brightness);
void leds_set_all_off(void);  // xxx probably not needed
void leds_rotate(int mode);

void leds_show(int all_brightness);

// -------- t2s.c --------

void t2s_init(void);

void t2s_play_text(char *text);

// -------- wwd.c --------

#define WW_KEYWORD  0
#define WW_TERMINATE 1

void wwd_init(void);

int wwd_feed(short sound_val);

// -------- doa.c --------

void doa_init(void);

void doa_feed(const float * frame);
double doa_get(void);

// -------- s2t.c --------

void s2t_init(void);
char * s2t_feed(short sound_val);


// -------- grammar.c --------

typedef char args_t[10][1000];
typedef int (*hndlr_t)(args_t args);

typedef struct {
    char *name;
    hndlr_t proc;
} hndlr_lookup_t;

int grammar_init(char *filename, hndlr_lookup_t *hlu);
bool grammar_match(char *cmd, hndlr_t *proc, args_t args);

