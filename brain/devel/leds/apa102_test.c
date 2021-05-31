#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>

// XXX add code to set GPIO5

// notes: 
// - respeaker requires gpio5 to be set, to enable the leds.
// - code is ported from ../repos/4mics_hat/apa102.py

#define NUM_LED 12

static struct {
    unsigned char hdr[4];
    struct led_s {
        unsigned char start_and_brightness;
        unsigned char blue;
        unsigned char green;
        unsigned char red;
    } led[NUM_LED];
    unsigned char trailer[(NUM_LED+15)/16];
} tx;

void set_leds(int brightness);

int main(int argc, char **argv)
{
    int fd, brightness, i;

    fd = open("/dev/spidev0.1", O_RDWR);
    if (fd < 0) {
        perror("open");
        exit(1);  
    }

    printf("Test1: ramp up brightness ...\n");
    for (brightness = 0; brightness < 32; brightness++) {
        set_leds(brightness);
        write(fd, &tx, sizeof(tx));
        usleep(100000);
    }

    printf("Test2: rotate ...\n");
    for (i = 0; i < 20; i++) {
        struct led_s tmp = tx.led[0];
        memmove(&tx.led[0], &tx.led[1], (NUM_LED-1)*sizeof(struct led_s));
        tx.led[NUM_LED-1] = tmp;
        write(fd, &tx, sizeof(tx));
        usleep(250000);
    }

    printf("Test3: ramp down brightness ...\n");
    for (brightness = 31; brightness >= 0; brightness--) {
        set_leds(brightness);
        write(fd, &tx, sizeof(tx));
        usleep(100000);
    }

    return 0;
}

void set_leds(int brightness)
{
    int i;

    //printf("set brightness %d\n", brightness);

    if (brightness < 0 || brightness > 31) {
        printf("ERROR: invalid brightness %d\n", brightness);
        exit(1);
    }

    for (i = 0; i < NUM_LED; i++) {
        tx.led[i].start_and_brightness = 0xe0 | brightness;
        tx.led[i].red   = (i % 3) == 0 ? 32 : 0;
        tx.led[i].green = (i % 3) == 1 ? 32 : 0;
        tx.led[i].blue  = (i % 3) == 2 ? 32 : 0;
    }
}
