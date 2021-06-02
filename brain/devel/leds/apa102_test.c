#include <stdio.h>
#include <unistd.h>

#include <apa102.h>
#include <gpio.h>

#define MAX_LED 12

unsigned int colors[] = {
        LED_WHITE, LED_RED, LED_PINK, LED_ORANGE, LED_YELLOW,
        LED_GREEN, LED_BLUE, LED_LIGHT_BLUE, LED_PURPLE, /*LED_OFF*/ };

#define MAX_COLORS (sizeof(colors)/sizeof(colors[0]))

int main(int argc, char **argv)
{
    int i, wavelen, brightness;

    if (apa102_init(MAX_LED) < 0) {
        return 1;
    }

    // enable VCC of the leds, this is required on respeaker
    if (gpio_init() < 0) {
        return 1;
    }
    set_gpio_func(5,FUNC_OUT);
    gpio_write(5,1);

    printf("Colors test ...\n");
    for (i = 0; i < MAX_COLORS; i++) {
        apa102_set_all_leds(colors[i], 10);
        apa102_show_leds();
        sleep(1);
    }
    apa102_set_all_leds_off();
    apa102_show_leds();
    sleep(1);

    printf("Wavelen test ...\n");
    for (wavelen = 400; wavelen <= 700; wavelen += 2) {
        unsigned int rgb = apa102_wavelen_to_rgb(wavelen);
        apa102_set_all_leds(rgb, 10);
        apa102_show_leds();
        usleep(100000);
    }
    apa102_set_all_leds_off();
    apa102_show_leds();
    sleep(1);

    printf("Brightness test ...\n");
    for (brightness = 0; brightness < MAX_BRIGHTNESS; brightness++) {
        for (i = 0; i < MAX_LED; i++) {
            apa102_set_led(i, colors[i%MAX_COLORS], brightness);
        }
        apa102_show_leds();
        usleep(200000);
    }
    apa102_set_all_leds_off();
    apa102_show_leds();

    return 0;
}
