// The Grove - 10A DC Current Sensor (ACS725) can measure the DC current 
// up to 10A and has a base sensitivity of 264mV/A. This sensor do not 
// support AC current.

#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>

#include <current.h>
#include <STM32_adc.h>
#include <misc.h>

static struct info_s {
    int adc_chan;
    double current;
} info_tbl[10];
static int max_info;

static void * current_thread(void *cx);

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

int current_read_unsmoothed(int id, double *current)
{
    double v;

    STM32_adc_read(info_tbl[id].adc_chan, &v);
    *current = (v - 0.322) * (1. / .264);

    return 0;
}

int current_read(int id, double *current)
{
    *current = info_tbl[id].current;

    return 0;
}

// -----------------  THREAD-------------------------------------

static void * current_thread(void *cx)
{
    int id;
    double current;

    while (true) {
        for (id = 0; id < max_info; id++) {
            current_read_unsmoothed(id, &current);

            info_tbl[id].current = 0.975 * info_tbl[id].current + 
                                   0.025 * current;

            usleep(20000);   // 20 ms
        }
    }

    return NULL;
}

