#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <math.h>

#include <imu.h>
#include <MPU9250_imu.h>
#include <misc.h>

#define ACCEL_ALERT 1.3   // XXX param

// defines

#define MAG_CAL_FILENAME "MPU9250_imu_magnetometer.cal"

// variables

static int mx_cal, my_cal, mz_cal;
static int mx, my, mz;

// prototypes

static void * magnetometer_thread(void *cx);
static void * accelerometer_thread(void *cx);

// -----------------  API  --------------------------------------

int imu_init(int dev_addr)  // multiple instances not supported
{
    static pthread_t mag_tid;
    static pthread_t accel_tid;

    // check if already initialized
    if (mag_tid) {
        ERROR("already initialized\n");
        return -1;
    }

    // init MPU9250 imu device
    if (MPU9250_imu_init(0) < 0) {
        ERROR("MPU9250_imu_init failed\n");
        return -1;
    }

    // read magnetometer calibratin file
    FILE *fp = fopen(MAG_CAL_FILENAME, "r");
    if (fp == NULL) {
        ERROR("failed to open %s\n", MAG_CAL_FILENAME);
        return -1;
    }
    if (fscanf(fp, "%d %d %d", &mx_cal, &my_cal, &mz_cal) != 3) {
        ERROR("%s invalid format\n", MAG_CAL_FILENAME);
        return -1;
    }
    fclose(fp);
    INFO("read calibration values: %d %d %d\n", mx_cal, my_cal, mz_cal);

    // create threads to read the magnetometer and accelerometer
    pthread_create(&mag_tid, NULL, magnetometer_thread, NULL);
    pthread_create(&accel_tid, NULL, accelerometer_thread, NULL);

    // return success
    return 0;
}

void imu_read_magnetometer(double *heading_arg)
{
    double heading;

    heading = atan2(-(my-my_cal), mx-mx_cal) * (180 / M_PI);
    if (heading < 0) heading += 360;

    *heading_arg = heading;
}

// -----------------  THREAD-------------------------------------

static void * magnetometer_thread(void *cx)
{
    int new_mx, new_my, new_mz;

    while (true) {
        MPU9250_imu_get_magnetometer(&new_mx, &new_my, &new_mz);

        mx = 0.9 * mx + 0.1 * new_mx;
        my = 0.9 * my + 0.1 * new_my;
        mz = 0.9 * mz + 0.1 * new_mz;

        usleep(10000);  // 10 ms
    }

    return NULL;
}

static void * accelerometer_thread(void *cx)
{
    int ax, ay, az;
    double axd, ayd, azd;
    double a_total_squared;
    uint64_t t_last_print = microsec_timer();
    uint64_t t_now;

    while (true) {
        MPU9250_imu_get_acceleration(&ax, &ay, &az);

        axd = (double)(ax-1200) / 16384;
        ayd = (double)(ay-1200) / 16384;
        azd = (double)(az-1200) / 16384;
        a_total_squared = axd*axd + ayd*ayd + azd*azd;

        t_now = microsec_timer();
        if (t_now - t_last_print > 1000000) {
            INFO("accel = %0.1f\n", sqrt(a_total_squared));
            t_last_print = t_now;
        }

        if (a_total_squared > (ACCEL_ALERT*ACCEL_ALERT)) {
            INFO("\aALERT: ax,ay,az = %5.2f %5.2f %5.2f  total = %5.2f\n",
                   axd, ayd, azd, sqrt(a_total_squared));
            // XXX pass this up
        }

        usleep(10000);  // 10 ms
    }

    return NULL;
}
