#ifndef __PROXIMITY_H__
#define __PROXIMITY_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

#define DEFAULT_PROXIMITY_ALERT_LIMIT  0.1

// Notes:
// - proximity_init varargs: int gpio_pin_sig, int gpio_pin_enable, ...
// - call to proximity_set_alert_limit sets the limit for all proximity sensors

int proximity_init(int max_info, ...);  // returns -1 on error, else 0

bool proximity_check(int id, double *avg_sig);

void proximity_enable(int id);
void proximity_disable(int id);
void proximity_set_alert_limit(double alert_limit);

bool proximity_get_enabled(int id);
double proximity_get_alert_limit(void);
int proximity_get_poll_intvl_us(void);

#ifdef __cplusplus
}
#endif

#endif
