#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <apa102.h>
#include <gpio.h>

#define MAX_LED 12

unsigned int colors[] = {
        LED_WHITE, LED_RED, LED_PINK, LED_ORANGE, LED_YELLOW,
        LED_GREEN, LED_BLUE, LED_LIGHT_BLUE, LED_PURPLE, /*LED_OFF*/ };

#define MAX_COLORS (sizeof(colors)/sizeof(colors[0]))

int main(int argc, char **argv)
{
    int i, wavelen, led_brightness, all_brightness;

    // init apa102 led device
    if (apa102_init(MAX_LED) < 0) {
        return 1;
    }

    // if 'off' requested then exit, leds are now off due to above call to apa102_init
    if (argc == 2 && strcmp(argv[1], "off") == 0) {
        return 1;
    }

    // enable VCC of the leds, this is required on respeaker
    if (gpio_init() < 0) {
        return 1;
    }
    set_gpio_func(5,FUNC_OUT);
    gpio_write(5,1);

    // tests follow ...
    printf("Colors test ...\n");
    for (i = 0; i < MAX_COLORS; i++) {
        apa102_set_all_leds(colors[i], 100);
        apa102_show_leds(31);
        sleep(1);
    }
    apa102_show_leds(0);
    sleep(1);

    printf("Wavelen test ...\n");
    for (wavelen = 400; wavelen <= 700; wavelen += 2) {
        unsigned int rgb = apa102_wavelen_to_rgb(wavelen);
        apa102_set_all_leds(rgb, 100);
        apa102_show_leds(31);
        usleep(100000);
    }
    apa102_show_leds(0);
    sleep(1);

    printf("LED brightness test ...\n");
    int color = LED_WHITE;
    for (led_brightness = 0; led_brightness <= 100; led_brightness++) {
        for (i = 0; i < MAX_LED; i++) {
            apa102_set_led(i, color, led_brightness);
        }
        apa102_show_leds(31);
        usleep(100000);
    }
    apa102_show_leds(0);
    sleep(1);

    printf("All brightness test ...\n");
    for (all_brightness = 0; all_brightness <= 31; all_brightness++) {
        apa102_show_leds(all_brightness);
        usleep(100000);
    }
    for (all_brightness = 31; all_brightness >= 0; all_brightness--) {
        apa102_show_leds(all_brightness);
        usleep(100000);
    }

    printf("Rotate test ...\n");
    for (i = 0; i < MAX_LED; i++) {
        apa102_set_led(i, 
                       LED_GREEN, 
                       i * 100 / (MAX_LED-1));
    }
    for (int cnt = 0; cnt < 100; cnt++) {
        apa102_rotate_leds(0);
        apa102_show_leds(31);
        usleep(100000);
    }
    for (int cnt = 0; cnt < 100; cnt++) {
        apa102_rotate_leds(1);
        apa102_show_leds(31);
        usleep(100000);
    }

    // done
    return 0;
}
