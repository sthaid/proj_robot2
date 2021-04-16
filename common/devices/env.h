#ifndef __ENV_H__
#define __ENV_H__

#ifdef __cplusplus
extern "C" {
#endif

int env_init(int dev_addr);  // multiple instances not supported
int env_read(double *temperature, double *pressure);   // args are optional

#ifdef __cplusplus
}
#endif

#endif

