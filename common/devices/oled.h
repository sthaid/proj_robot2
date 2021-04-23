#ifndef __OLED_H__
#define __OLED_H__

#ifdef __cplusplus
extern "C" {
#endif

// Notes:
// - oled_init varargs: int dev_addr, ...
// - dev_addr==0 means to use the default dev_addr of the oled device

int oled_init(int max_info, ...);  // returns -1 on error, else 0
void oled_draw_str(int id, char *str);

#ifdef __cplusplus
}
#endif

#endif

