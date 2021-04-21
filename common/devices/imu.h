#ifndef __IMU_H__
#define __IMU_H__

#ifdef __cplusplus
extern "C" {
#endif

#define DEFAULT_ACCEL_ALERT_LIMIT 1.3
#define MAGNETOMETER_CAL_FILENAME "imu_magnetometer.cal"

#include <stdbool.h>

int imu_init(int dev_addr);  // multiple instances not supported

void imu_read_magnetometer(double *heading);

void imu_set_accel_alert_limit(double accel_alert_limit);
void imu_check_accel_alert(bool *accel_alert, double *accel_alert_value);

#ifdef __cplusplus
}
#endif

#endif

