// XXX file hdr

#define MAX_BRIGHTNESS 32

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

int apa102_init(int max_led);

void apa102_set_led(int num, unsigned int rgb, int brightness);
void apa102_set_all_leds(unsigned int rgb, int brightness);
void apa102_set_all_leds_off(void);

void apa102_show_leds(void);

unsigned int apa102_wavelen_to_rgb(double wavelength);
