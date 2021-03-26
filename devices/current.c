// The Grove - 10A DC Current Sensor (ACS725) can measure the DC current 
// up to 10A and has a base sensitivity of 264mV/A. This sensor do not 
// support AC current.

#include <unistd.h>

#include <current.h>
#include <STM32_adc.h>

int current_init(void)
{
    return 0;
}

int current_read(int adc_chan, double *current)
{
    int n;
    double v_sum, v_avg, v;

    v_sum = 0;
    for (n = 0; n < 200; n++) {
        STM32_adc_read(adc_chan, &v);
        v_sum += v;
        usleep(2000);
    }
    v_avg = v_sum / n;

    *current = (v_avg - 0.322) * (1. / .264);

    return 0;
}

