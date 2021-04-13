#include "STM32_adc.h"
#include "../i2c/i2c.h"

// Notes:
// - this ADC is on the Seeed Grove Base Hat for Rapsberry Pi
// - range 0 - 3.3 V

#define STM32_ADC_DEFAULT_ADDR  0x04
#define STM32_ADC_REG_VOLTAGE   0x20

static int dev_addr;

// -----------------  C LANGUAGE API  -----------------------------------

int STM32_adc_init(int dev_addr_arg)
{
    // set dev_addr
    dev_addr = (dev_addr_arg == 0 ? STM32_ADC_DEFAULT_ADDR : dev_addr_arg);

    // init i2c
    if (i2c_init() < 0) {
        return -1;
    }

    return 0;
}

int STM32_adc_read(int chan, double *voltage)
{
    uint8_t data[2];

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

// -----------------  C LANGUAGE TEST PROGRAM  ---------------------------

#ifdef TEST

#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    if (STM32_adc_init(0) < 0) {
        printf("STM32_adc_init failed\n");
        return 1;
    }

    while (true) {
        printf("Voltage: ");
        for (int chan = 0; chan < 8; chan++) {
            double voltage;
            STM32_adc_read(chan, &voltage);
            printf("%6.3f ", voltage);
        }
        printf("\n");
        sleep(1);
    }

    return 0;
}

#endif
