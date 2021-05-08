#ifndef __MPU9250_IMU_H__
#define __MPU9250_IMU_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

int MPU9250_imu_init(int dev_addr);

int MPU9250_imu_get_acceleration(int *ax_arg, int *ay_arg, int *az_arg);
int MPU9250_imu_get_rotation(int *x_arg, int *y_arg, int *z_arg);
int MPU9250_imu_get_accel_and_rot(int *ax_arg, int *ay_arg, int *az_arg,
                                  int *rx_arg, int *ry_arg, int *rz_arg);

int MPU9250_imu_get_magnetometer(int *mx_arg, int *my_arg, int *mz_arg);
int MPU9250_imu_calibrate_magnetometer(int *mx_cal, int *my_cal, int *mz_cal);
double MPU9250_imu_mag_to_heading(int mx, int my, int mx_cal, int my_cal);

#ifdef __cplusplus
}
#endif

#endif
