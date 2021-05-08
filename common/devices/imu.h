#ifndef __IMU_H__
#define __IMU_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

// Notes: 
// - multiple instances not supported

int imu_init(int dev_addr);  // return -1 on error, else 0

// magnetometer
double imu_get_magnetometer(void);
#define MAG_CAL_CTRL_DISABLED        0
#define MAG_CAL_CTRL_DISABLED_SAVE   1
#define MAG_CAL_CTRL_ENABLED         2
void imu_set_mag_cal_ctrl(int ctrl);
bool imu_get_mag_cal_ctrl(void);

// accel and rotation enable/disable ctrl
void imu_set_accel_rot_ctrl(bool enable);
bool imu_get_accel_rot_ctrl(void);

// accel elert
void imu_set_accel_alert_limit(double accel_alert_limit);
double imu_get_accel_alert_limit(void);
bool imu_check_accel_alert(double *accel_alert_value);  // optional arg

// rotation
double imu_get_rotation(void);
void imu_reset_rotation(void);

#ifdef __cplusplus
}
#endif

#endif

