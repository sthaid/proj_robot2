#ifndef __PA_UTIL_H__
#define __PA_UTIL_H__

#define DEFAULT_OUTPUT_DEVICE "DEFAULT_OUTPUT_DEVICE"
#define DEFAULT_INPUT_DEVICE  "DEFAULT_INPUT_DEVICE"

// note - these must match portaudio defines paFloat32, etc
#define PA_FLOAT32       (0x00000001) 
#define PA_INT32         (0x00000002) 
#define PA_INT24         (0x00000004) 
#define PA_INT16         (0x00000008) 
#define PA_INT8          (0x00000010) 
#define PA_UINT8         (0x00000020)

typedef int (*play2_get_frame_t)(void *data, void *cx);
typedef int (*record2_put_frame_t)(const void *data, void *cx);

int pa_init(void);

int pa_play(char *output_device, int max_chan, int max_data, int sample_rate, int sample_format, void *data);
int pa_play2(char *output_device, int max_chan, int sample_rate, int sample_format, play2_get_frame_t get_frame, void *get_frame_cx);

int pa_record(char *input_device, int max_chan, int max_data, int sample_rate, int sample_format, void *data, int discard_samples);
int pa_record2(char *input_device, int max_chan, int sample_rate, int sample_format, record2_put_frame_t put_frame, void *put_frame_cx, int discard_samples);

int pa_find_device(char *name);
void pa_print_device_info(int idx);
void pa_print_device_info_all(void);

#endif
