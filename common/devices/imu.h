#ifndef __IMU_H__
#define __IMU_H__

#ifdef __cplusplus
extern "C" {
#endif

int imu_init(int dev_addr);  // multiple instances not supported
void imu_read_magnetometer(double *heading);

#ifdef __cplusplus
}
#endif

#endif

