#ifndef __ENV_H__
#define __ENV_H__

#ifdef __cplusplus
extern "C" {
#endif

// Note: multiple instances not supported

int env_init(int dev_addr);  // return -1 on error, else 0

double env_read_temperature(void);
double env_read_pressure(void);

#ifdef __cplusplus
}
#endif

#endif

