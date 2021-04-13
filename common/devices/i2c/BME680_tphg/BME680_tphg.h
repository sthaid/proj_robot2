#ifndef __BME680_TPH_H__
#define __BME680_TPH_H__

#ifdef __cplusplus
extern "C" {
#endif

int BME680_tphg_init(int dev_addr);
int BME680_tphg_read(double *temperature, double *pressure, double *humidity, double *gas_resistance);

#ifdef __cplusplus
}
#endif

#endif
