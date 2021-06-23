#ifndef __PA_UTIL_H__
#define __PA_UTIL_H__

#define DEFAULT_OUTPUT_DEVICE "DEFAULT_OUTPUT_DEVICE"
#define DEFAULT_INPUT_DEVICE  "DEFAULT_INPUT_DEVICE"

typedef int (*play2_get_frame_t)(float *data, void *cx);

int pa_init(void);

int pa_play(char *output_device, int max_chan, int max_data, int sample_rate, float *data);
int pa_play2(char *output_device, int max_chan, int sample_rate, play2_get_frame_t get_frame, void *get_frame_cx);

int pa_record(char *input_device, int max_chan, int max_data, int sample_rate, float *data);

int pa_find_device(char *name);
void pa_print_device_info(int idx);
void pa_print_device_info_all(void);

#endif
