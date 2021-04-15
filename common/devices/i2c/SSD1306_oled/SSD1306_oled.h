#ifndef __SSD1306_OLDED_H__
#define __SSD1306_OLDED_H__

#ifdef __cplusplus
extern "C" {
#endif

int SSD1306_oled_init(int dev_addr);
int SSD1306_oled_drawstr(int dev_addr, int x, int y, char *s);

#ifdef __cplusplus
}
#endif

#endif
