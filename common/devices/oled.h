#ifndef __OLED_H__
#define __OLED_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>

int oled_init(int max_info, ...);  // int dev_addr, ...
void oled_set_str(int id, int stridx, char *str);
void oled_set_intvl_us(int id, unsigned int intvl_us);
void oled_set_next(int id);

#ifdef __cplusplus
}
#endif

#endif

