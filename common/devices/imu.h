#ifndef __IMU_H__
#define __IMU_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

// Notes: 
// - multiple instances not supported
// - imu_check_accel_alert, accel_alert_value arg is optional

int imu_init(int dev_addr);  // return -1 on error, else 0

double imu_read_magnetometer(void);

void imu_accel_enable(void);
void imu_accel_disable(void);
bool imu_get_accel_enabled(void);
void imu_set_accel_alert_limit(double accel_alert_limit);
double imu_get_accel_alert_limit(void);
bool imu_check_accel_alert(double *accel_alert_value);

#ifdef __cplusplus
}
#endif

#endif

