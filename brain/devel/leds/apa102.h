#ifndef __APA102_H__
#define __APA102_H__

#ifdef __cplusplus
extern "C" {
#endif

#define LED_RGB(r,g,b) ((unsigned int)(((r) << 0) | ((g) << 8) | ((b) << 16)))

#define LED_WHITE      LED_RGB(255,255,255)
#define LED_RED        LED_RGB(255,0,0)
#define LED_PINK       LED_RGB(255,105,180)
#define LED_ORANGE     LED_RGB(255,128,0)
#define LED_YELLOW     LED_RGB(255,255,0)
#define LED_GREEN      LED_RGB(0,255,0)
#define LED_BLUE       LED_RGB(0,0,255)
#define LED_LIGHT_BLUE LED_RGB(0,255,255)
#define LED_PURPLE     LED_RGB(127,0,255)
#define LED_OFF        LED_RGB(0,0,0)

// notes:
// - led_brightness range     0 - 100
// - all_led_brightness range 0 - 31

int apa102_init(int max_led);

void apa102_set_led(int num, unsigned int rgb, int led_brightness);
void apa102_set_all_leds(unsigned int rgb, int led_brightness);
void apa102_set_all_leds_off(void);
void apa102_rotate_leds(int mode);

void apa102_show_leds(int all_led_brightness);

unsigned int apa102_wavelen_to_rgb(double wavelength);

#ifdef __cplusplus
}
#endif

#endif

