// The Grove - 10A DC Current Sensor (ACS725) can measure the DC current 
// up to 10A and has a base sensitivity of 264mV/A. This sensor do not 
// support AC current.

#include <unistd.h>
#include <stdbool.h>

#include <current.h>
#include <STM32_adc.h>
#include <misc.h>

static struct info_s {
    int adc_chan;
} info_tbl[10];
int max_info;

// -----------------  API  --------------------------------------

int current_init(int max_info_arg, ...)  // int adc_chan, ...
{
    static bool initialized;
    va_list ap;

    // check if already initialized
    if (initialized) {
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

    // set initialized flag
    initialized = true;

    // return success
    return 0;
}

int current_read(int id, double *current)
{
    int n;
    double v_sum, v_avg, v;

    v_sum = 0;
    for (n = 0; n < 200; n++) {
        STM32_adc_read(info_tbl[id].adc_chan, &v);
        v_sum += v;
        usleep(2000);
    }
    v_avg = v_sum / n;

    *current = (v_avg - 0.322) * (1. / .264);

    return 0;
}

