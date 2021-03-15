#include <stdio.h>
#include <math.h>

#include "../../util/misc.h"
#include "../MCP9808_temp/MCP9808_temp.h"
#include "../STM32_adc/STM32_adc.h"
#include "../BME680_tphg/BME680_tphg.h"
#include "../SSD1306_oled/SSD1306_oled.h"
#include "../MPU9250_imu/MPU9250_imu.h"
#include "../BMP280_tp/BMP280_tp.h"

int main(int argc, char **argv)
{
    double degc;

    //INFO("Starting\n");

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

    if (SSD1306_oled_init(0) < 0) {
        ERROR("SSD1306_oled_init failed\n");
        return 1;
    }

    if (MPU9250_imu_init(0) < 0) {
        ERROR("MPU9250_imu_init failed\n");
        return 1;
    }

    if (BMP280_tp_init(0) < 0) {
        ERROR("BMP280_tp_init failed\n");
        return 1;
    }


    // - - - - - - - - - - - - - - - 

    MCP9808_temp_read(&degc);
    printf("Temperature: %0.1f\n", degc);

    printf("Voltage:    ");
    for (int chan = 0; chan < 8; chan++) {
        double voltage;
        STM32_adc_read(chan, &voltage);
        printf("%6.3f ", voltage);
    }
    printf("\n");

    double temperature, pressure, humidity;
    BME680_tphg_read(&temperature, &pressure, &humidity, NULL);
    printf("TPH:         %0.1f C   %0.0f Pa   %0.2f inch-Hg   %0.1f %%\n", 
           temperature, 
           pressure, pressure/3386,
           humidity);

    SSD1306_oled_drawstr("WORLD");

    BMP280_tp_read(&temperature, &pressure);
    printf("TP:          %0.1f C   %0.0f Pa   %0.2f inch-Hg\n", 
           temperature, 
           pressure, pressure/3386);


    int16_t ax, ay, az;
    int16_t gx, gy, gz;
    int16_t mx, my, mz;
    ax = ay = az = 0;
    gx = gy = gz = 0;
    mx = my = mz = 0;
    MPU9250_imu_getmotion9(&ax, &ay, &az, &gx, &gy, &gz, &mx, &my, &mz);
    printf("ax,ay,az = %d %d %d\n", ax, ay, az);
    printf("gx,gy,gz = %d %d %d\n", gx, gy, gz);
    printf("mx,my,mz = %d %d %d\n", mx, my, mz);

    double  Axyz[3];
    double  Gxyz[3];
    double  Mxyz[3];

    Axyz[0] = (double) ax / 16384;
    Axyz[1] = (double) ay / 16384;
    Axyz[2] = (double) az / 16384;

    Gxyz[0] = (double) gx * 250 / 32768;
    Gxyz[1] = (double) gy * 250 / 32768;
    Gxyz[2] = (double) gz * 250 / 32768;

    Mxyz[0] = (double) mx * 1200 / 4096;
    Mxyz[1] = (double) my * 1200 / 4096;
    Mxyz[2] = (double) mz * 1200 / 4096;

    printf("ax,ay,az = %5.2f %5.2f %5.2f\n", Axyz[0], Axyz[1], Axyz[2]);
    printf("gx,gy,gz = %5.2f %5.2f %5.2f\n", Gxyz[0], Gxyz[1], Gxyz[2]);
    printf("mx,my,mz = %5.2f %5.2f %5.2f\n", Mxyz[0], Mxyz[1], Mxyz[2]);

    printf("The clockwise angle between the magnetic north and X-Axis\n");

    double heading = 180 * atan2(Mxyz[1], Mxyz[0]) / M_PI;
    if (heading < 0) heading += 360;
    printf("%6.1f\n", heading);



    return 0;
}
