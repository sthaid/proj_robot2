#ifndef __RELAY_H__
#define __RELAY_H__

#ifdef __cplusplus
extern "C" {
#endif

int relay_init(void);
void relay_ctrl(int pin, bool enable);

#ifdef __cplusplus
}
#endif

#endif
