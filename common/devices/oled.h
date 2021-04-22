#ifndef __OLED_H__
#define __OLED_H__

#ifdef __cplusplus
extern "C" {
#endif

int oled_init(int max_info, ...);  // returns -1 on error, else 0

void oled_set_str(int id, int stridx, char *str);
void oled_set_intvl_us(int id, unsigned int intvl_us);
void oled_set_next(int id);

void oled_get_strs(int id, int *max, char *strs[]);

#ifdef __cplusplus
}
#endif

#endif

