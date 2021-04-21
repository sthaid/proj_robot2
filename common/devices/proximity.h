#ifndef __PROXIMITY_H__
#define __PROXIMITY_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

#define DEFAULT_PROXIMITY_ALERT_LIMIT  0.1

int proximity_init(int max_info, ...);  // int gpio_sig, int gpio_enable, ...

void proximity_check(int id, bool *alert, double *avg_sig);
void proximity_enable(int id);
void proximity_disable(int id);

void proximity_set_alert_limit(double alert_limit);   // applies to all prox sensors
void proximity_get_poll_intvl_us(int *poll_intvl_us);  // debug routine

#ifdef __cplusplus
}
#endif

#endif
