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
#define MAG_CAL_FILENAME "imu_mag.cal"

// prototypes

static void * magnetometer_thread(void *cx);
static int read_mag_cal_file(void);
static int write_mag_cal_file(void);

static void * accel_rot_thread(void *cx);
static void process_raw_accel_values(int ax, int ay, int az);
static void process_raw_rot_values(int rx, int ry, int rz);

// -----------------  INIT  -------------------------------------

int imu_init(int dev_addr)  // multiple instances not supported
{
    static pthread_t mag_tid;
    static pthread_t accel_rot_tid;

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
    if (read_mag_cal_file() < 0) {
        WARN("failed to read magnetometer calibration file\n");
    }

    // create threads to read the magnetometer and accelerometer/rotation
    pthread_create(&mag_tid, NULL, magnetometer_thread, NULL);
    pthread_create(&accel_rot_tid, NULL, accel_rot_thread, NULL);

    // return success
    return 0;
}

// -----------------  MAGNETOMETER  ---------------------------- 

static int mag_cal_ctrl;
static double mx_cal, my_cal, mz_cal;
static double mx_smoothed, my_smoothed, mz_smoothed;

double imu_get_magnetometer(void)
{
    double heading;

    heading = atan2(-(my_smoothed-my_cal), mx_smoothed-mx_cal) * (180 / M_PI);
    if (heading < 0) heading += 360;
    return heading;
}

void imu_set_mag_cal_ctrl(int ctrl)
{
    mag_cal_ctrl = ctrl;
}

bool imu_get_mag_cal_ctrl(void)
{
    return mag_cal_ctrl;
}
    
static void * magnetometer_thread(void *cx)
{
    int mx_raw, my_raw, mz_raw;
    int mx_cal_min, my_cal_min, mz_cal_min, mx_cal_max, my_cal_max, mz_cal_max;
    int mag_cal_ctrl_lcl;
    int mag_cal_ctrl_last = MAG_CAL_CTRL_DISABLED;

    mx_cal_min = my_cal_min = mz_cal_min = +1000000;
    mx_cal_max = my_cal_max = mz_cal_max = -1000000;

    while (true) {
        // read raw magnetometer values
        MPU9250_imu_get_magnetometer(&mx_raw, &my_raw, &mz_raw);

        // publish smoothed magnetometer values
        mx_smoothed = 0.9 * mx_smoothed + 0.1 * mx_raw;
        my_smoothed = 0.9 * my_smoothed + 0.1 * my_raw;
        mz_smoothed = 0.9 * mz_smoothed + 0.1 * mz_raw;

        // if mag_cal_ctrl was changed then
        //   if it is now enabled then reset vars
        //   if it is now set to 'save' then save vars
        // else if mag_cal is enabled
        //   keep track of min/max values
        // endif
        mag_cal_ctrl_lcl = mag_cal_ctrl;
        if (mag_cal_ctrl_lcl != mag_cal_ctrl_last) {
            if (mag_cal_ctrl_lcl == MAG_CAL_CTRL_ENABLED) {
                mx_cal_min = my_cal_min = mz_cal_min = +1000000;
                mx_cal_max = my_cal_max = mz_cal_max = -1000000;
            } else if (mag_cal_ctrl_lcl == MAG_CAL_CTRL_DISABLED_SAVE) {
                mx_cal = (mx_cal_max + mx_cal_min) / 2.;
                my_cal = (my_cal_max + my_cal_min) / 2.;
                mz_cal = (mz_cal_max + mz_cal_min) / 2.;
                write_mag_cal_file();
            }
        } else if (mag_cal_ctrl_lcl == MAG_CAL_CTRL_ENABLED) {
            if (mx_raw < mx_cal_min) mx_cal_min = mx_raw;
            if (my_raw < my_cal_min) my_cal_min = my_raw;
            if (mz_raw < mz_cal_min) mz_cal_min = mz_raw;

            if (mx_raw > mx_cal_max) mx_cal_max = mx_raw;
            if (my_raw > my_cal_max) my_cal_max = my_raw;
            if (mz_raw > mz_cal_max) mz_cal_max = mz_raw;
        }
        mag_cal_ctrl_last = mag_cal_ctrl_lcl;

        // delay 10 ms
        usleep(10000);
    }

    return NULL;
}

static int read_mag_cal_file(void)
{
    FILE *fp;

    fp = fopen(MAG_CAL_FILENAME, "r");
    if (fp == NULL) {
        ERROR("failed to open %s\n", MAG_CAL_FILENAME);
        return -1;
    }

    if (fscanf(fp, "%lf %lf %lf", &mx_cal, &my_cal, &mz_cal) != 3) {
        ERROR("invalid format %s\n", MAG_CAL_FILENAME);
        return -1;
    }
    fclose(fp);
    INFO("read calibration values: %0.1f %0.1f %0.1f\n", mx_cal, my_cal, mz_cal);

    return 0;
}

static int write_mag_cal_file(void)
{
    FILE *fp;

    fp = fopen(MAG_CAL_FILENAME, "w");
    if (fp == NULL) {
        ERROR("failed to open %s\n", MAG_CAL_FILENAME);
        return 1;
    }

    fprintf(fp, "%0.1f %0.1f %0.1f\n", mx_cal, my_cal, mz_cal);
    fclose(fp);
    INFO("write calibration values: %0.1f %0.1f %0.1f\n", mx_cal, my_cal, mz_cal);

    return 0;
}

// -----------------  ACCEL AND ROTATION  ---------------------- 

static bool accel_rot_enabled;

static bool accel_alert;
static double accel_alet_g_value;
static double accel_alert_limit = DEFAULT_ACCEL_ALERT_LIMIT;

static double rotation;
static double rotation_offset;

void imu_set_accel_rot_ctrl(bool enable)
{
    accel_alet_g_value = 0;
    accel_alert = 0;
    rotation = 0;
    rotation_offset = 0;
    __sync_synchronize();

    accel_rot_enabled = enable;
}

bool imu_get_accel_rot_ctrl(void)
{
    return accel_rot_enabled;
}

// - - - - - - - - -  acceleration   - - - - - - - - - - - - - - -

void imu_set_accel_alert_limit(double accel_alert_limit_arg)
{
    accel_alert_limit = accel_alert_limit_arg;
}

double imu_get_accel_alert_limit(void)
{
    return accel_alert_limit;
}

bool imu_check_accel_alert(double *accel_alet_g_value_arg)
{
    bool ret;

    ret = accel_alert;
    if (accel_alet_g_value_arg) {
        *accel_alet_g_value_arg = accel_alet_g_value;
    }

    accel_alet_g_value = 0;
    accel_alert = 0;

    return ret;
}

// - - - - - - - - -  rotation  - - - - - - - - - - - - - - - - -

double imu_get_rotation(void)
{
    return rotation - rotation_offset;
}

void imu_reset_rotation(void)
{
    rotation_offset = rotation;
}

// - - - - - - - - -  acceleration and rotation thread  - - - - -

static void * accel_rot_thread(void *cx)
{
    int ax, ay, az;
    int rx, ry, rz;

    while (true) {
        // if accel/rotation monitoring is not enabled then delay and continue,
        // skipping the processing that follows
        if (!accel_rot_enabled) {
            usleep(10000);
            continue;
        }

        // read raw acceleromter and rotation values from i2c device
        MPU9250_imu_get_accel_and_rot(&ax, &ay, &az, &rx, &ry, &rz);

        // process accel and rotation values
        process_raw_accel_values(ax, ay, az);
        process_raw_rot_values(rx, ry, rz);

        // sleep 10 ms
        usleep(10000);
    }

    return NULL;
}

static void process_raw_accel_values(int ax, int ay, int az)
{
    double axd, ayd, azd;
    double accel_total_squared;

    // convert raw values to g units
    axd = (double)(ax-1200) / 16384;
    ayd = (double)(ay-1200) / 16384;
    azd = (double)(az-1200) / 16384;
    accel_total_squared = axd*axd + ayd*ayd;

#ifdef DEBUG_ACCEL
    // debug print every 10 secs
    static uint64_t t_last_print;
    uint64_t t_now = microsec_timer();
    if (t_now - t_last_print > 10000000) {
        INFO("accel = %0.2f  (%0.2f %0.2f %0.2f)\n", 
              sqrt(accel_total_squared), axd, ayd, azd);
        t_last_print = t_now;
    }
#endif

    // if large accel detected then set the accel_alert flag
    if (accel_total_squared > (accel_alert_limit * accel_alert_limit)) {
        accel_alert = true;
        accel_alet_g_value = sqrt(accel_total_squared);
        INFO("ALERT: ax,ay,az = %5.2f %5.2f %5.2f  total = %5.2f\n",
             axd, ayd, azd, accel_alet_g_value);
    }
}

static void process_raw_rot_values(int rx, int ry, int rz)
{
    uint64_t time_now_us = microsec_timer();
    double delta_t;
    static uint64_t time_last_us;

    delta_t = (time_now_us - time_last_us) / 1000000.;
    time_last_us = time_now_us;
    if (delta_t > .03) {
        WARN("discarding value because delta_t %0.3f secs is too large\n", delta_t);
        return;
    }

    rotation += (rz * (-1./131.) + 0.067) * delta_t;
}
