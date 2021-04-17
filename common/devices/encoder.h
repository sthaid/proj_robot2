#ifndef __ENCODER_H__
#define __ENCODER_H__

#ifdef __cplusplus
extern "C" {
#endif

int encoder_init(int max_info, ...);

void encoder_get_position(int id, int *position);
void encoder_get_speed(int id, int *speed);
void encoder_get_errors(int id, int *errors);
void encoder_pos_reset(int id);
void encoder_enable(int id);
void encoder_disable(int id);

void encoder_get_poll_intvl_us(int *poll_intvl_us);  // debug routine


#ifdef __cplusplus
}
#endif

#endif
