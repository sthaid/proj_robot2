#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

int MPU9250_imu_init(int dev_addr);
int MPU9250_imu_getmotion9(int16_t *ax, int16_t *ay, int16_t *az,
                           int16_t *gx, int16_t *gy, int16_t *gz,
                           int16_t *mx, int16_t *my, int16_t *mz);

#ifdef __cplusplus
}
#endif
