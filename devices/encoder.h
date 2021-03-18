#ifndef __ENCODER_H__
#define __ENCODER_H__

#ifdef __cplusplus
extern "C" {
#endif

int encoder_init(void);
void encoder_get(int id, int *position, int *speed);

#ifdef __cplusplus
}
#endif

#endif
