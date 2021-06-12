#ifndef __PA_UTIL_H__
#define __PA_UTIL_H__

extern char *DEFAULT_OUTPUT_DEVICE;
extern char *DEFAULT_INPUT_DEVICE;

PaDeviceIndex pa_find_device(char *name);
void pa_print_device_info(PaDeviceIndex idx);
void pa_print_device_info_all(void);

#endif
