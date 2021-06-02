#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>   // not currently used

#include <apa102.h>
#include <misc.h>

// XXX TODO
// - move to common dir and tests dir
// - add rotate func

// developed using info from:
//   brain/devel/repos/4mics_hat/apa102.py

//
// defines
//

#define SPIDEV "/dev/spidev0.1"

#define LED_BRIGHTNESS_FUDGE 0.5

//
// variables
//

static int fd;

static struct {
    unsigned char hdr[4];  // hdr bytes are set to zero
    struct led_s {
        unsigned char start_and_brightness;
        unsigned char blue;
        unsigned char green;
        unsigned char red;
    } led[0];
    // unsigned char trailer[max_led+15)/16], set to zero
} *tx;

static int tx_buff_size;

static int max_led;

// ---------------------------------------------------------------------------------------

int apa102_init(int max_led_arg)
{
    // open spi device
    fd = open("/dev/spidev0.1", O_RDWR);
    if (fd < 0) {
        ERROR("open %s failed, %s\n", SPIDEV, strerror(errno));
        exit(1);
    }

    // a short delay after opening seems to eliminate 
    // initial erroneous led colors 
    usleep(200000);

    // save max_led_arg in global
    max_led = max_led_arg;

    // allocate tx buff, including space for:
    // - header:  4 bytes
    // - led[]:   max_led * sizeof(struct_led_s) bytes
    // - trailer: (max_led+15)/16 bytes
    // the header and trailer are set to 0
    tx_buff_size =  4 + max_led*sizeof(struct led_s) + (max_led+15)/16;
    tx = malloc(tx_buff_size);
    memset(tx, 0, tx_buff_size);

    // set leds off, 
    apa102_set_all_leds_off();
    apa102_show_leds(0);

    // success
    return 0;
}

// - - - - - - - - - - 

void apa102_set_led(int num, unsigned int rgb, int led_brightness)
{
    struct led_s *x = &tx->led[num];

    if (num < 0 || num >= max_led) {
        ERROR("invalid arg num=%d\n", num);
        return;
    }

    if (led_brightness < 0 || led_brightness > 100) {
        ERROR("invalid arg led_brightnesss=%d\n", led_brightness);
        return;
    }

    double b = (double)led_brightness * (.01 * LED_BRIGHTNESS_FUDGE);
    x->red   = nearbyint(((rgb >>  0) & 0xff) * b);
    x->green = nearbyint(((rgb >>  8) & 0xff) * b);
    x->blue  = nearbyint(((rgb >> 16) & 0xff) * b);
}

void apa102_set_all_leds(unsigned int rgb, int led_brightness)
{
    int red, green, blue;

    if (led_brightness < 0 || led_brightness > 100) {
        ERROR("invalid arg led_brightnesss=%d\n", led_brightness);
        return;
    }

    double b = (double)led_brightness * (.01 * LED_BRIGHTNESS_FUDGE);
    red   = nearbyint(((rgb >>  0) & 0xff) * b);
    green = nearbyint(((rgb >>  8) & 0xff) * b);
    blue  = nearbyint(((rgb >> 16) & 0xff) * b);

    for (int num = 0; num < max_led; num++) {
        struct led_s *x = &tx->led[num];
        x->red   = red;
        x->green = green;
        x->blue  = blue;
    }
}

void apa102_set_all_leds_off(void)
{
    memset(tx, 0, tx_buff_size);
}

void apa102_show_leds(int all_led_brightness)
{
    if (all_led_brightness < 0 || all_led_brightness > 31) {
        ERROR("invalid arg all_led_brightnesss=%d\n", all_led_brightness);
        return;
    }

    for (int num = 0; num < max_led; num++) {
        struct led_s *x = &tx->led[num];
        x->start_and_brightness = 0xe0 | all_led_brightness;
    }

    write(fd, tx, tx_buff_size);
}

// - - - - - - - - - - 

// ported from http://www.noah.org/wiki/Wavelength_to_RGB_in_Python
unsigned int apa102_wavelen_to_rgb(double wavelength)
{
    double attenuation;
    double gamma = 0.8;
    double R,G,B;

    if (wavelength >= 380 && wavelength <= 440) {
        double attenuation = 0.3 + 0.7 * (wavelength - 380) / (440 - 380);
        R = pow((-(wavelength - 440) / (440 - 380)) * attenuation, gamma);
        G = 0.0;
        B = pow(1.0 * attenuation, gamma);
    } else if (wavelength >= 440 && wavelength <= 490) {
        R = 0.0;
        G = pow((wavelength - 440) / (490 - 440), gamma);
        B = 1.0;
    } else if (wavelength >= 490 && wavelength <= 510) {
        R = 0.0;
        G = 1.0;
        B = pow(-(wavelength - 510) / (510 - 490), gamma);
    } else if (wavelength >= 510 && wavelength <= 580) {
        R = pow((wavelength - 510) / (580 - 510), gamma);
        G = 1.0;
        B = 0.0;
    } else if (wavelength >= 580 && wavelength <= 645) {
        R = 1.0;
        G = pow(-(wavelength - 645) / (645 - 580), gamma);
        B = 0.0;
    } else if (wavelength >= 645 && wavelength <= 750) {
        attenuation = 0.3 + 0.7 * (750 - wavelength) / (750 - 645);
        R = pow(1.0 * attenuation, gamma);
        G = 0.0;
        B = 0.0;
    } else {
        R = 0.0;
        G = 0.0;
        B = 0.0;
    }

    if (R < 0) R = 0; else if (R > 1) R = 1;
    if (G < 0) G = 0; else if (G > 1) G = 1;
    if (B < 0) B = 0; else if (B > 1) B = 1;

    return ((int)nearbyint(R*255) <<  0) |
           ((int)nearbyint(G*255) <<  8) |
           ((int)nearbyint(B*255) << 16);
}
