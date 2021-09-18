
// XXX re-order

// -------- misc.c --------

uint64_t microsec_timer(void);
char *time2str(time_t t, char *s);
unsigned int wavelen_to_rgb(double wavelength);
void run_program(pid_t *prog_pid, int *fd_to_prog, int *fd_from_prog, char *prog, ...);

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
void leds_set_all_off(void);
void leds_rotate(int mode);

void leds_show(int all_brightness);

// -------- t2s.c --------

void t2s_init(void);

void t2s_play_text(char *text);

// -------- wwd.c --------

#define WW_PORCUPINE  0
#define WW_TERMINATOR 1

void wwd_init(void);

int wwd_feed(short sound_val);

// -------- doa.c --------

void doa_init(void);

void doa_feed(const float * frame);

// -------- s2t.c --------

void s2t_init(void);
char * s2t_feed(short sound_val);
