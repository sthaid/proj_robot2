#include <config_hw.h>
#include <STM32_adc.h>
#include <current.h>
#include <misc.h>

int main(int argc, char **argv)
{
    double current;
    double min=100, max=-100;

    if (STM32_adc_init(0) < 0) {
        ERROR("STM32_adc_init failed\n");
        return 1;
    }
    if (current_init() < 0) {
        ERROR("current_init failed\n");
        return 1;
    }

    while (true) {
        current_read(CURRENT_ADC_CHAN, &current);
        if (current < min) min = current;
        if (current > max) max = current;
        INFO("current = %5.2f   range = %5.2f ... %5.2f\n", current, min, max);
    }

    return 0;
}
