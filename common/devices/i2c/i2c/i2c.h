#ifndef __I2C_H__
#define __I2C_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

int i2c_init(void);

int i2c_read(int dev_addr, uint8_t reg_addr, uint8_t *reg_data, int len);
int8_t i2c_read_data(uint8_t dev_addr, uint8_t *reg_data, uint16_t len);

int i2c_write(int dev_addr, uint8_t reg_addr, uint8_t * reg_data, int len);
void i2c_delay_ns(unsigned int ns);

#ifdef __cplusplus
}
#endif

#endif
