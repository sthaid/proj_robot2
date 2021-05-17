#ifndef __ENCODER_H__
#define __ENCODER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

// Notes:
// - encoder_init varargs: int gpio_pin_a, int gpio_pin_b, ...

int encoder_init(int max_info, ...);   // return -1 on error, else 0

void encoder_enable(int id);
void encoder_disable(int id);
void encoder_count_reset(int id);

bool encoder_get_enabled(int id);     // these return values
int encoder_get_count(int id);
int encoder_get_speed(int id);
int encoder_get_errors(int id);
int encoder_get_poll_intvl_us(void);

#ifdef __cplusplus
}
#endif

#endif
