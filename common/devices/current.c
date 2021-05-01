// The Grove - 10A DC Current Sensor (ACS725) can measure the DC current 
// up to 10A and has a base sensitivity of 264mV/A. This sensor do not 
// support AC current.

#include <unistd.h>
#include <stdbool.h>
#include <stdarg.h>
#include <pthread.h>

#include <current.h>
#include <STM32_adc.h>
#include <misc.h>

static struct info_s {
    int adc_chan;
    double current_smoothed;
} info_tbl[10];
static int max_info;

static void * current_thread(void *cx);
static double read_current_unsmoothed(int id);

// -----------------  API  --------------------------------------

int current_init(int max_info_arg, ...)  // int adc_chan, ...
{
    static pthread_t tid;
    va_list ap;

    // check if already initialized
    if (tid) {
        ERROR("already initialized\n");
        return -1;
    }

    // init ADC I2C device XXX can this be called multiple times
    if (STM32_adc_init(0) < 0) {
        ERROR("STM32_adc_init failed\n");
        return -1;
    }

    // save hardware info
    va_start(ap, max_info_arg);
    for (int i = 0; i < max_info_arg; i++) {
        info_tbl[i].adc_chan = va_arg(ap, int);
    }
    max_info = max_info_arg;
    va_end(ap);

    // create thread
    pthread_create(&tid, NULL, current_thread, NULL);

    // return success
    return 0;
}

double current_get(int id)
{
    return info_tbl[id].current_smoothed;
}

// -----------------  THREAD-------------------------------------

static void * current_thread(void *cx)
{
    while (true) {
        for (int id = 0; id < max_info; id++) {
            info_tbl[id].current_smoothed = 
                0.98 * info_tbl[id].current_smoothed + 
                0.02 * read_current_unsmoothed(id);
        }
        usleep(10000);   // 10 ms
    }

    return NULL;
}

static double read_current_unsmoothed(int id)
{
    double v;

    STM32_adc_read(info_tbl[id].adc_chan, &v);
    return (v - 0.322) * (1. / .264);
}
