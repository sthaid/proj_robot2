#include "STM32_adc.h"
#include "../common/i2c.h"

// XXX comment Seeed HAT

// range 0 - 3.3 V

#define STM32_ADC_DEFAULT_ADDR  0x04
#define STM32_ADC_REG_VOLTAGE   0x20

int STM32_adc_init(int dev_addr)
{
    if (dev_addr == 0) {
        dev_addr = STM32_ADC_DEFAULT_ADDR;
    }

    if (i2c_init() < 0) {
        return -1;
    }

    return 0;
}

int STM32_adc_read(int dev_addr, int chan, double *voltage)
{
    uint8_t data[2];

    if (dev_addr == 0) {
        dev_addr = STM32_ADC_DEFAULT_ADDR;
    }

    if (chan < 0 || chan > 8) {
        return -1;
    }

    if (i2c_read(dev_addr, STM32_ADC_REG_VOLTAGE+chan, data, 2) < 0) {
        *voltage = 0;
        return -1;
    }

    *voltage = ((data[1] << 8) | data[0]) / 1000.;
    return 0;
}
