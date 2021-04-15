#ifndef __PROXIMITY_H__
#define __PROXIMITY_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>
#include <stdbool.h>

int proximity_init(int max_info, ...);  // int gpio_sig, int gpio_enable, ...
void proximity_enable(int id);
void proximity_disable(int id);
bool proximity_check(int id, double *avg_sig, int *poll_rate);

#ifdef __cplusplus
}
#endif

#endif
