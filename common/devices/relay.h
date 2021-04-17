#ifndef __RELAY_H__
#define __RELAY_H__

#ifdef __cplusplus
extern "C" {
#endif

int relay_init(int max_info, ...);  // int gpio_pin, ...
void relay_ctrl(int id, bool enable);

#ifdef __cplusplus
}
#endif

#endif
