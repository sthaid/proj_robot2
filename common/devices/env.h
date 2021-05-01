#ifndef __ENV_H__
#define __ENV_H__

#ifdef __cplusplus
extern "C" {
#endif

#define PASCAL_TO_INHG(pa)          ((pa) * 0.0002953)
#define CENTIGRADE_TO_FAHRENHEIT(c) ((c) * 1.8 + 32.)

// Note: multiple instances not supported

int env_init(int dev_addr);  // return -1 on error, else 0

double env_get_temperature_degc(void);
double env_get_pressure_pascal(void);
double env_get_temperature_degf(void);
double env_get_pressure_inhg(void);

#ifdef __cplusplus
}
#endif

#endif

