#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
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
#include <dirent.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <wiringPi.h>

#define MB 0x100000LL
#define GB (1024 * MB)
#define PAGE_SIZE  (sysconf(_SC_PAGE_SIZE))

#define SECONDS 1000000
#define MS      1000

// -------- logging.c  ------

#define INFO(fmt, args...) log_msg("INFO", fmt, ## args);
#define WARN(fmt, args...) log_msg("WARN", fmt, ## args);
#define ERROR(fmt, args...) log_msg("ERROR", fmt, ## args);

#define INFO_INTVL(us, fmt, args...) \
    do { \
        static uint64_t last; \
        uint64_t now = microsec_timer(); \
        if (now - last > (us)) { \
            log_msg("INFO", fmt, ## args); \
            last = now; \
        } \
    } while (0)
#define WARN_INTVL(us, fmt, args...) \
    do { \
        static uint64_t last; \
        uint64_t now = microsec_timer(); \
        if (now - last > (us)) { \
            log_msg("WARN", fmt, ## args); \
            last = now; \
        } \
    } while (0)
#define ERROR_INTVL(us, fmt, args...) \
    do { \
        static uint64_t last; \
        uint64_t now = microsec_timer(); \
        if (now - last > (us)) { \
            log_msg("ERROR", fmt, ## args); \
            last = now; \
        } \
    } while (0)

#define VERBOSE0(fmt, args...) do { if (log_verbose[0]) log_msg("VERBOSE0", fmt, ## args); } while (0)
#define VERBOSE1(fmt, args...) do { if (log_verbose[1]) log_msg("VERBOSE1", fmt, ## args); } while (0)
#define VERBOSE2(fmt, args...) do { if (log_verbose[2]) log_msg("VERBOSE2", fmt, ## args); } while (0)
#define VERBOSE3(fmt, args...) do { if (log_verbose[3]) log_msg("VERBOSE3", fmt, ## args); } while (0)

#define FATAL(fmt, args...) do { log_msg("FATAL", fmt, ## args); exit(1); } while (0)

#define MAX_VERBOSE 4
bool log_verbose[MAX_VERBOSE];

void log_init(char *filename, bool append, bool brief);
void log_msg(char *lvl, char *fmt, ...) __attribute__ ((format (printf, 2, 3)));

// -------- misc.c --------

void misc_init(void);

uint64_t microsec_timer(void);
char *time2str(time_t t, char *s);

int getsockaddr(char * node, int port, struct sockaddr_in * ret_addr);
char * sock_addr_to_str(char * s, int slen, struct sockaddr * addr);

unsigned int wavelen_to_rgb(double wavelength);

void run_program(pid_t *prog_pid, int *fd_to_prog, int *fd_from_prog, char *prog, ...);

void poly_fit(int max_data, double *x_data, double *y_data, int degree_of_poly, double *coefficients);

uint32_t crc32(const void *buf, size_t size);
uint32_t crc32_multi_buff(int n, ...);  // buff,sizeof(buff),repeat

double normalize_angle(double angle);
double max_doubles(double *x, int n, int *max_idx);
double min_doubles(double *x, int n, int *min_idx);
char *stars(double v, double max_v, int max_stars, char *s);
void shuffle(void *array, int elem_size, int num_elem);
int get_filenames(char *dirname, char **names, int *max_names);
int clip_int(int val, int min, int max);
double clip_double(double val, double min, double max);
bool strmatch(char *s, ...);

// -------- filter routines  --------

static inline double low_pass_filter(double v, double *cx, double k2)
{
    *cx = k2 * *cx + (1-k2) * v;
    return *cx;
}

static inline double low_pass_filter_ex(double v, double *cx, int k1, double k2)
{
    for (int i = 0; i < k1; i++) {
        v = low_pass_filter(v, &cx[i], k2);
    }
    return v;
}

static inline double high_pass_filter(double v, double *cx, double k2)
{
    *cx = *cx * k2 + (1-k2) * v;
    return v - *cx;
}

static inline double high_pass_filter_ex(double v, double *cx, int k1, double k2)
{
    for (int i = 0; i < k1; i++) {
        v = high_pass_filter(v, &cx[i], k2);
    }
    return v;
}

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

// -------- sf.c --------

void sf_init(void);

int sf_write_wav_file(char *filename, short *data, int max_chan, int max_data, int sample_rate);
int sf_read_wav_file(char *filename, short **data, int *max_chan, int *max_data, int *sample_rate);
int sf_read_wav_file2(char *filename, short *data, int *max_chan, int *max_data, int *sample_rate);

int sf_gen_sweep_wav(char *filename, int freq_start, int freq_end, int duration, int max_chan, int sample_rate);
int sf_gen_white_wav(char *filename, int duration, int max_chan, int sample_rate);

// -------- leds.c --------

// notes:
// - led_brightness range  0 - 100
// - all_brightness range  0 - 100
// - mode: 0=counterclockwise, 1=clockwise (on respeaker)

#define MAX_LED 12

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

void leds_init(double sf);  // xxx notes on sf

void leds_set_scale_factor(double sf);
void leds_stage_led(int num, unsigned int rgb, int led_brightness);
void leds_stage_all(unsigned int rgb, int led_brightness);
void leds_stage_rotate(int mode);

void leds_commit(int all_brightness);

// -------- s2t.c --------

void s2t_init(void);

char *s2t_feed(short sound_val);

// -------- t2s.c --------

void t2s_init(void);

void t2s_play(char *fmt, ...) __attribute__((format(printf, 1, 2)));
void t2s_play_nocache(char *fmt, ...) __attribute__((format(printf, 1, 2)));

// -------- wwd.c --------

#define WW_KEYWORD_MASK    1
#define WW_TERMINATE_MASK  2

void wwd_init(void);

int wwd_feed(short sound_val);

// -------- doa.c --------

void doa_init(void);

void doa_feed(const short *frame);
double doa_get(void);

// -------- grammar.c --------

typedef char args_t[10][200];
typedef int (*hndlr_t)(args_t args);

typedef struct {
    char *name;
    hndlr_t proc;
} hndlr_lookup_t;

int grammar_init(char *filename, hndlr_lookup_t *hlu);

bool grammar_match(char *cmd, hndlr_t *proc, args_t args);

// -------- db.c --------

void db_init(char *file_name, bool create, uint64_t file_len);

int db_get(int keyid, char *keystr, void **val, unsigned int *val_len);
int db_set(int keyid, char *keystr, void *val, unsigned int val_len);
int db_rm(int keyid, char *keystr);
int db_get_keyid(int keyid, void (*callback)(int keyid, char *keystr, void *val, unsigned int val_len));

void db_set_num(int keyid, char *keystr, double value);
double db_get_num(int keyid, char *keystr, double default_value);

void db_print_free_list(void);
unsigned int db_get_free_list_len(void);
void db_reset(void);
void db_dump(void);

// -------- audio.c --------

#define AUDIO_SHM "/audio_shm"

#define AUDIO_OUT_STATE_IDLE      0
#define AUDIO_OUT_STATE_PREP      1
#define AUDIO_OUT_STATE_PLAY      2
#define AUDIO_OUT_STATE_PLAY_DONE 3

typedef struct {
    // audio input ...
    short frames[48000][4];
    int   fidx;
    bool  reset_mic;
    // audio output ...
    short data[3600*24000];
    int   max_data;
    int   sample_rate;
    int   state;
    bool  cancel;
    bool  complete_to_idle;
    // audio output amplitude of low, mid and high freq ranges
    double low;
    double mid;
    double high;
} audio_shm_t;

void audio_init(int (*proc_mic_data)(short *frame), int volume);

int audio_in_reset_mic(void);

void audio_out_beep(int beep_count, bool complete_to_idle);
void audio_out_play_data(short *data, int max_data, int sample_rate, bool complete_to_idle);
void audio_out_play_wav(char *file_name, bool complete_to_idle);

void audio_out_wait(void);
bool audio_out_is_complete(bool *cancelled);
void audio_out_cancel(void);
void audio_out_set_state_idle(void);
void audio_out_get_low_mid_high(double *low, double *mid, double *high);

void audio_out_set_volume(int volume);

