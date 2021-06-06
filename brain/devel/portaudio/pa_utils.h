#ifndef __PA_UTIL_H__
#define __PA_UTIL_H__

#define PA_ERROR_CHECK(rc, routine_name) \
    do { \
        if (rc != paNoError) { \
            printf("ERROR: %s rc=%d, %s\n", routine_name, rc, Pa_GetErrorText(rc)); \
            Pa_Terminate(); \
            exit(1); \
        } \
    } while (0)

void print_device_info(PaDeviceIndex idx);
void print_device_info_all(void);

#endif
