#ifndef __PA_UTIL_H__
#define __PA_UTIL_H__

#define DEFAULT_OUTPUT_DEVICE "DEFAULT_OUTPUT_DEVICE"
#define DEFAULT_INPUT_DEVICE  "DEFAULT_INPUT_DEVICE"

int pa_play(char *output_device, int max_chan, int max_data, int sample_rate, float **chan_data);
int pa_init(void);

int pa_find_device(char *name);
void pa_print_device_info(int idx);
void pa_print_device_info_all(void);

#endif
