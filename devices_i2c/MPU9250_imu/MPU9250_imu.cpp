#include "MPU9250_imu.h"
#include "mpu9250/MPU9250.h"
#include "../i2c/i2c.h"

#define MPU9250_IMU_DEFAULT_ADDR  0x68

class MPU9250 *mpu9250;

extern "C" {

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

int MPU9250_imu_get_motion9(int16_t *ax, int16_t *ay, int16_t *az,
                            int16_t *gx, int16_t *gy, int16_t *gz,
                            int16_t *mx, int16_t *my, int16_t *mz)
{
    mpu9250->getMotion9(ax, ay, az, gx, gy, gz, mx, my, mz);
    return 0;
}

int MPU9250_imu_get_acceleration(int16_t *ax, int16_t *ay, int16_t *az)
{
    mpu9250->getAcceleration(ax, ay, az);
    return 0;
}

} // extern "C"
