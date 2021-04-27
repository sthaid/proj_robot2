#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <math.h>

#include <imu.h>
#include <MPU9250_imu.h>
#include <misc.h>

// defines

//#define DEBUG_ACCEL

#define DEFAULT_ACCEL_ALERT_LIMIT 1.5
#define MAGNETOMETER_CAL_FILENAME "imu_magnetometer.cal"

// variables

static int mx_cal, my_cal, mz_cal;
static int mx, my, mz;

static bool accel_enabled;
static bool accel_alert;
static double accel_alert_value;
static double accel_alert_limit = DEFAULT_ACCEL_ALERT_LIMIT;

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
    FILE *fp = fopen(MAGNETOMETER_CAL_FILENAME, "r");
    if (fp == NULL) {
        ERROR("failed to open %s\n", MAGNETOMETER_CAL_FILENAME);
        return -1;
    }
    if (fscanf(fp, "%d %d %d", &mx_cal, &my_cal, &mz_cal) != 3) {
        ERROR("%s invalid format\n", MAGNETOMETER_CAL_FILENAME);
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

// - - - - - - - - -  MAGNETOMETER   - - - - - - - - - - - - - - 

double imu_read_magnetometer(void)
{
    double heading;

    heading = atan2(-(my-my_cal), mx-mx_cal) * (180 / M_PI);
    if (heading < 0) heading += 360;
    return heading;
}

// - - - - - - - - -  ACCELEROMETER  - - - - - - - - - - - - - - 

// enable ctrl routines 

void imu_accel_enable(void)
{
    accel_alert_value = 0;
    accel_alert = 0;
    accel_enabled = true;
}

void imu_accel_disable(void)
{
    accel_alert_value = 0;
    accel_alert = 0;
    accel_enabled = false;
}

bool imu_get_accel_enabled(void)
{
    return accel_enabled;
}

// accel_alert_limit set/get routines

void imu_set_accel_alert_limit(double accel_alert_limit_arg)
{
    accel_alert_limit = accel_alert_limit_arg;
}

double imu_get_accel_alert_limit(void)
{
    return accel_alert_limit;
}

// check for accel alert

bool imu_check_accel_alert(double *accel_alert_value_arg)
{
    bool ret;

    ret = accel_alert;
    if (accel_alert_value_arg) {
        *accel_alert_value_arg = accel_alert_value;
    }

    accel_alert_value = 0;
    accel_alert = 0;

    return ret;
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
    double accel_total_squared;
#ifdef DEBUG_ACCEL
    uint64_t t_now, t_last_print=microsec_timer();
#endif

    while (true) {
        // if accel monitoring is not enabled then delay and continue,
        // skipping the processing that follows
        if (!accel_enabled) {
            usleep(10000);
            continue;
        }

        // read raw acceleromter values from i2c device
        MPU9250_imu_get_acceleration(&ax, &ay, &az);

        // convert raw values to g units
        // note: XXX do not include z accel for now, 
        //       further investigation needed
        axd = (double)(ax-1200) / 16384;
        ayd = (double)(ay-1200) / 16384;
        azd = (double)(az-1200) / 16384;
        accel_total_squared = axd*axd + ayd*ayd;

#ifdef DEBUG_ACCEL
        // debug print every 10 secs
        t_now = microsec_timer();
        if (t_now - t_last_print > 10000000) {
            INFO("accel = %0.2f  (%0.2f %0.2f %0.2f)\n", 
                  sqrt(accel_total_squared), axd, ayd, azd);
            t_last_print = t_now;
        }
#endif

        // if large accel detected then set the accel_alert flag
        if (accel_total_squared > (accel_alert_limit * accel_alert_limit)) {
            accel_alert = true;
            accel_alert_value = sqrt(accel_total_squared);
            INFO("ALERT: ax,ay,az = %5.2f %5.2f %5.2f  total = %5.2f\n",
                 axd, ayd, azd, accel_alert_value);
        }

        // sleep 10 ms
        usleep(10000);
    }

    return NULL;
}
