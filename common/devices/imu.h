#ifndef __IMU_H__
#define __IMU_H__

#ifdef __cplusplus
extern "C" {
#endif

#define DEFAULT_ACCEL_ALERT_LIMIT 2.0
#define MAGNETOMETER_CAL_FILENAME "imu_magnetometer.cal"

#include <stdbool.h>

// Notes: 
// - multiple instances not supported
// - imu_check_accel_alert, accel_alert_value arg is optional

int imu_init(int dev_addr);  // return -1 on error, else 0

double imu_read_magnetometer(void);

bool imu_check_accel_alert( double *accel_alert_value);
void imu_set_accel_alert_limit(double accel_alert_limit);
double imu_get_accel_alert_limit(void);

#ifdef __cplusplus
}
#endif

#endif

