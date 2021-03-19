#ifndef __ENCODER_H__
#define __ENCODER_H__

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_ENCODER 1

int encoder_init(void);
void encoder_get(int id, int *position, int *speed);
void encoder_get_ex(int id, int *position, int *speed, int *errors, int *poll_rate);

#ifdef __cplusplus
}
#endif

#endif
