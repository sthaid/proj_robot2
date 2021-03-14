#include <stdio.h>

#include "../../util/misc.h"
#include "../MCP9808_temp/MCP9808_temp.h"
#include "../STM32_adc/STM32_adc.h"
#include "../BME680_tphg/BME680_tphg.h"

int main(int argc, char **argv)
{
    double degc;

    INFO("Starting\n");

    if (MCP9808_temp_init(0) < 0) {
        ERROR("MCP9808_temp_init failed\n");
        return 1;
    }

    if (STM32_adc_init(0) < 0) {
        ERROR("STM32_adc_init failed\n");
        return 1;
    }

    if (BME680_tphg_init(0) < 0) {
        ERROR("BME680_tphg_init failed\n");
        return 1;
    }

    MCP9808_temp_read(0, &degc);
    printf("Temperature: %0.1f\n", degc);

    printf("Voltage:    ");
    for (int chan = 0; chan < 8; chan++) {
        double voltage;
        STM32_adc_read(0, chan, &voltage);
        printf("%6.3f ", voltage);
    }
    printf("\n");

    double temperature, pressure, humidity;
    BME680_tphg_read(0, &temperature, &pressure, &humidity, NULL);
    printf("TPH:         %0.1f C   %0.0f Pa   %0.2f inch-Hg   %0.1f %%\n", 
           temperature, 
           pressure, pressure/3386,
           humidity);

    return 0;
}
