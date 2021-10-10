#include <utils.h>

unsigned int colors[] = {
        LED_WHITE, LED_RED, LED_PINK, LED_ORANGE, LED_YELLOW,
        LED_GREEN, LED_BLUE, LED_LIGHT_BLUE, LED_PURPLE, /*LED_OFF*/ };

#define MAX_COLORS (sizeof(colors)/sizeof(colors[0]))

#define LEDS_ALL_OFF \
    do { \
        leds_stage_all(LED_OFF,0); \
        leds_commit(); \
    } while (0)

int main(int argc, char **argv)
{
    int i, wavelen, led_brightness;

    // use stdout for log msgs
    logging_init(NULL,false);

    // init leds led device
    leds_init();

    // if 'off' requested then exit, leds are now off due to above call to leds_init
    if (argc == 2 && strcmp(argv[1], "off") == 0) {
        return 1;
    }

    // tests follow ...
    INFO("Colors test ...\n");
    for (i = 0; i < MAX_COLORS; i++) {
        leds_stage_all(colors[i], 100);
        leds_commit();
        sleep(1);
    }
    LEDS_ALL_OFF;
    sleep(1);

    INFO("Wavelen test ...\n");
    for (wavelen = 400; wavelen <= 700; wavelen += 2) {
        unsigned int rgb = wavelen_to_rgb(wavelen);
        leds_stage_all(rgb, 100);
        leds_commit();
        usleep(100000);
    }
    LEDS_ALL_OFF;
    sleep(1);

    INFO("LED brightness test ...\n");
    int color = LED_WHITE;
    for (led_brightness = 0; led_brightness <= 100; led_brightness++) {
        for (i = 0; i < MAX_LED; i++) {
            leds_stage_led(i, color, led_brightness);
        }
        leds_commit();
        usleep(100000);
    }
    LEDS_ALL_OFF;
    sleep(1);

    INFO("Rotate test ...\n");
    for (i = 0; i < MAX_LED; i++) {
        leds_stage_led(i, 
                       LED_LIGHT_BLUE, 
                       i * 100 / (MAX_LED-1));
    }
    for (int cnt = 0; cnt < 100; cnt++) {
        leds_stage_rotate(0);
        leds_commit();
        usleep(100000);
    }
    for (int cnt = 0; cnt < 100; cnt++) {
        leds_stage_rotate(1);
        leds_commit();
        usleep(100000);
    }

    // done
    LEDS_ALL_OFF;
    return 0;
}
