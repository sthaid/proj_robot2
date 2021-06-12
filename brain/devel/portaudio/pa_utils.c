#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <portaudio.h>
#include <pa_utils.h>

char *DEFAULT_OUTPUT_DEVICE = "DEFAULT_OUTPUT_DEVICE";
char *DEFAULT_INPUT_DEVICE  = "DEFAULT_INPUT_DEVICE";

PaDeviceIndex pa_find_device(char *name)
{
    int dev_cnt = Pa_GetDeviceCount();
    int i;

    if (strcmp(name, DEFAULT_OUTPUT_DEVICE) == 0) {
	return Pa_GetDefaultOutputDevice();
    }
    if (strcmp(name, DEFAULT_INPUT_DEVICE) == 0) {
	return Pa_GetDefaultInputDevice();
    }

    for (i = 0; i < dev_cnt; i++) {
        const PaDeviceInfo *di = Pa_GetDeviceInfo(i);
        if (di == NULL) {
            return paNoDevice;
        }
        if (strncmp(di->name, name, strlen(name)) == 0) {
            return i;
        }
    }

    return paNoDevice;
}

void pa_print_device_info(PaDeviceIndex idx)
{
    const PaDeviceInfo *di;
    const PaHostApiInfo *hai;
    char host_api_info_str[100];

    di = Pa_GetDeviceInfo(idx);
    hai = Pa_GetHostApiInfo(di->hostApi);

    sprintf(host_api_info_str, "%s%s%s",
            hai->name,
            hai->defaultInputDevice == idx ? " DEFAULT_INPUT" : "",
            hai->defaultOutputDevice == idx ? " DEFAULT_OUTPUT" : "");

    printf("PaDeviceIndex = %d\n", idx);
    printf("  name                       = %s\n",    di->name);
    printf("  hostApi                    = %s\n",    host_api_info_str);
    printf("  maxInputChannels           = %d\n",    di->maxInputChannels);
    printf("  maxOutputChannels          = %d\n",    di->maxOutputChannels);
    printf("  defaultLowInputLatency     = %0.3f\n", di->defaultLowInputLatency);
    printf("  defaultLowOutputLatency    = %0.3f\n", di->defaultLowOutputLatency);
    printf("  defaultHighInputLatency    = %0.3f\n", di->defaultHighInputLatency);
    printf("  defaultHighOutputLatency   = %0.3f\n", di->defaultHighOutputLatency);
    printf("  defaultSampleRate          = %0.0f\n", di->defaultSampleRate);  // XXX get all rates
    printf("\n");
}

void pa_print_device_info_all(void)
{
    int i;
    int dev_cnt = Pa_GetDeviceCount();
    const PaHostApiInfo *hai = Pa_GetHostApiInfo(0);

    if (dev_cnt != hai->deviceCount) {
        printf("ERROR: BUG dev_cnt=%d hai->deviceCount=%d\n", dev_cnt, hai->deviceCount);
        exit(1);
    }

    printf("hostApi = %s  device_count = %d  default_input = %d  default_output = %d\n",
           hai->name,
           hai->deviceCount,
           hai->defaultInputDevice,
           hai->defaultOutputDevice);
    printf("\n");

    for (i = 0; i < dev_cnt; i++) {
        pa_print_device_info(i);
    }
}
