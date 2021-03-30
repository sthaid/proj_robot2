#include <time.h>
#include <unistd.h>
#include <math.h>

#include "MPU9250_imu.h"
#include "mpu9250/MPU9250.h"
#include "../i2c/i2c.h"

#define MPU9250_IMU_DEFAULT_ADDR  0x68

class MPU9250 *mpu9250;

extern "C" {

// -----------------  C LANGUAGE API  -----------------------------------

int MPU9250_imu_init(int dev_addr)
{
    // set dev_addr
    if (dev_addr == 0) {
        dev_addr = MPU9250_IMU_DEFAULT_ADDR;
    }

    // init i2c
    if (i2c_init() < 0) {
        return -1;
    }

    // create new mpu9250, and initialize
    mpu9250 = new MPU9250 (dev_addr);
    mpu9250->initialize();

    // success
    return 0;
}

int MPU9250_imu_get_acceleration(int *ax_arg, int *ay_arg, int *az_arg)
{
    int16_t ax, ay, az;

    mpu9250->getAcceleration(&ax, &ay, &az);
    *ax_arg = ax;
    *ay_arg = ay;
    *az_arg = az;
    return 0;
}

int MPU9250_imu_get_rotation(int *x_arg, int *y_arg, int *z_arg)
{
    int16_t x, y, z;

    mpu9250->getRotation(&x, &y, &z);
    *x_arg = x;
    *y_arg = y;
    *z_arg = z;
    return 0;
}

int MPU9250_imu_get_magnetometer(int *mx_arg, int *my_arg, int *mz_arg)
{
    int16_t mx, my, mz;

    mpu9250->getMagnetometer(&mx, &my, &mz);
    *mx_arg = mx;
    *my_arg = my;
    *mz_arg = mz;
    return 0;
}

int MPU9250_imu_calibrate_magnetometer(int *mx_cal, int *my_cal, int *mz_cal)
{
    time_t time_done;
    int16_t mx, my, mz;
    int     mx_min=+1000000, my_min=+1000000, mz_min=+1000000;
    int     mx_max=-1000000, my_max=-1000000, mz_max=-1000000;

    time_done = time(NULL) + 30;

    while (time(NULL) < time_done) {
        mpu9250->getMagnetometer(&mx, &my, &mz);

        if (mx < mx_min) mx_min = mx;
        if (my < my_min) my_min = my;
        if (mz < mz_min) mz_min = mz;

        if (mx > mx_max) mx_max = mx;
        if (my > my_max) my_max = my;
        if (mz > mz_max) mz_max = mz;

        usleep(20000);
    }

    *mx_cal = (mx_max + mx_min) / 2;
    *my_cal = (my_max + my_min) / 2;
    *mz_cal = (mz_max + mz_min) / 2;
    return 0;
}

double MPU9250_imu_mag_to_heading(int mx, int my, int mx_cal, int my_cal)
{
    double heading;

    heading = atan2(mx-mx_cal, my-my_cal) * (180 / M_PI);
    if (heading < 0) heading += 360;
    return heading;
}

} // extern "C"

// -----------------  C LANGUAGE TEST PROGRAM  ---------------------------

#ifdef TEST

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#define MAG_CAL_FILENAME "MPU9250_imu_magnetometer.cal"

int main(int argc, char **argv)
{
    if (argc != 2) {
        printf("USAGE: %s <cal|mag|accel>\n", argv[0]);
        return 1;
    }

    if (MPU9250_imu_init(0) < 0) {
        printf("ERROR: MPU9250_imu_init failed\n");
        return 1;
    }

    if (strcmp(argv[1], "cal") == 0) {
        int mx_cal, my_cal, mz_cal;

        // get magnetometer calibration  xxx explain
        MPU9250_imu_calibrate_magnetometer(&mx_cal, &my_cal, &mz_cal);

        // write magnetometer calibration file
        FILE *fp = fopen(MAG_CAL_FILENAME, "w");
        if (fp == NULL) {
            printf("ERROR failed to open %s\n", MAG_CAL_FILENAME);
            return 1;
        }
        fprintf(fp, "%d %d %d\n", mx_cal, my_cal, mz_cal);
        fclose(fp);
        printf("file %s created\a\n", MAG_CAL_FILENAME);
    } else if (strcmp(argv[1], "mag") == 0) {
        int mx_cal, my_cal, mz_cal;
        int mx, my, mz;
        double heading;

        // read magnetometer calibratin file
        FILE *fp = fopen(MAG_CAL_FILENAME, "r");
        if (fp == NULL) {
            printf("ERROR failed to open %s\n", MAG_CAL_FILENAME);
            return 1;
        }
        if (fscanf(fp, "%d %d %d", &mx_cal, &my_cal, &mz_cal) != 3) {
            printf("ERROR: %s invalid format\n", MAG_CAL_FILENAME);
            return 1;
        }
        fclose(fp);
        printf("read calibration values: %d %d %d\n", mx_cal, my_cal, mz_cal);

        // print heading once per second
        while (true) {
            MPU9250_imu_get_magnetometer(&mx, &my, &mz);
            heading = MPU9250_imu_mag_to_heading(mx,my,mx_cal,my_cal);
            printf("heading = %3.0f\n", heading);
            sleep(1);
        }
    } else if (strcmp(argv[1], "accel") == 0) {
        // xxx 
        printf("TBD\n");
    } else {
        printf("ERROR: invalid arg '%s'\n", argv[1]);
        return 1;
    }
}

#endif
