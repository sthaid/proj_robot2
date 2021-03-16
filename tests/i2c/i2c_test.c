#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#include <misc.h>
#include <MCP9808_temp.h>
#include <STM32_adc.h>
#include <BME680_tphg.h>
#include <SSD1306_oled.h>
#include <MPU9250_imu.h>
#include <BMP280_tp.h>

int main(int argc, char **argv)
{
    double degc;

    setlinebuf(stdout);
    printf("\a");

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

    // XXX improve user intfc
    {
        int16_t ax, ay, az;
        int16_t ax_last, ay_last, az_last;
        int16_t ax_delta, ay_delta, az_delta;
        time_t t, tlast;
        int count = 0;

        printf("ACCEL TEST\n");
        tlast = time(NULL);
        MPU9250_imu_get_acceleration(&ax_last, &ay_last, &az_last);
        ax_last -= 1200;
        ay_last -= 1200;
        az_last -= 1200;
        while (true) {
            MPU9250_imu_get_acceleration(&ax, &ay, &az);
            ax -= 1200;
            ay -= 1200;
            az -= 1200;

            ax_delta = abs(ax - ax_last);
            ay_delta = abs(ay - ay_last);
            az_delta = abs(az - az_last);

            ax_last = ax;
            ay_last = ay;
            az_last = az;

            if (ax_delta > 20000 || ay_delta > 20000 || az_delta > 20000) {
                printf("\a  DELTA       %d,%d,%d\n", ax_delta, ay_delta, az_delta);
                printf("  VALUES      %d,%d,%d\n", ax, ay, az);
                printf("  LAST_VALUES %d,%d,%d\n", ax_last, ay_last, az_last);
            }

            count++;
            usleep(10000);
            
            t = time(NULL);
            if (t != tlast) {
                printf("TIME %ld   COUNT %d  VALUES %d,%d,%d\n", t, count, ax, ay, az);
                tlast = t;
                count = 0;
            }
        }
    }


    int16_t ax, ay, az;
    int16_t gx, gy, gz;
    int16_t mx, my, mz;
    ax = ay = az = 0;
    gx = gy = gz = 0;
    mx = my = mz = 0;
    MPU9250_imu_get_motion9(&ax, &ay, &az, &gx, &gy, &gz, &mx, &my, &mz);
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
