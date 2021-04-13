#ifndef __ENCODER_H__
#define __ENCODER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>

int encoder_init(int max_info, ...);
void encoder_get_position(int id, int *position);
void encoder_get_speed(int id, int *speed);
void encoder_get_ex(int id, int *position, int *speed, int *errors, int *poll_rate);
void encoder_pos_reset(int id);

#ifdef __cplusplus
}
#endif

#endif
