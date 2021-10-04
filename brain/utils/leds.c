#include <utils.h>

#include <linux/spi/spidev.h>

// developed using info from:
//   brain/devel/repos/4mics_hat/leds.py

//
// defines
//

#define SPIDEV "/dev/spidev0.1"

#define MAX_LED 12

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
    // unsigned char trailer[MAX_LED+15)/16], set to zero
} *tx;

static int tx_buff_size;

// -----------------  LEDS_INIT  ---------------------------------------------------------

void leds_init(void)
{
    // open spi device
    fd = open("/dev/spidev0.1", O_RDWR);
    if (fd < 0) {
        FATAL("open %s failed, %s\n", SPIDEV, strerror(errno));
    }

    // the default speed does not work reliably, so reduce to 250 khz
    int max_spd_hz = 25000000;
    if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &max_spd_hz) < 0) {
        FATAL("ioctl SPI_IOC_WR_MAX_SPEED_HZ failed, %s\n", strerror(errno));
    }

    // allocate tx buff, including space for:
    // - header:  4 bytes
    // - led[]:   MAX_LED * sizeof(struct_led_s) bytes
    // - trailer: (MAX_LED+15)/16 bytes
    // the header and trailer are set to 0
    tx_buff_size =  4 + MAX_LED*sizeof(struct led_s) + (MAX_LED+15)/16;
    tx = malloc(tx_buff_size);
    memset(tx, 0, tx_buff_size);

    // set leds off, 
    leds_set_all_off();
    leds_show(0);
}

// -----------------  XXX  ---------------------------------------------------------------

void leds_set(int num, unsigned int rgb, int led_brightness)
{
    struct led_s *x = &tx->led[num];
    double b;

    if (num < 0 || num >= MAX_LED) {
        ERROR("invalid arg num=%d\n", num);
        return;
    }

    if (led_brightness < 0 || led_brightness > 100) {
        ERROR("invalid arg led_brightnesss=%d\n", led_brightness);
        return;
    }

    // xxx comment
    // xxx optimize
    if (led_brightness > 0) {
        b = 1e-6 * (led_brightness * led_brightness * led_brightness) + .002;
        if (b > 1) b = 1;
    } else {
        b = 0;
    }
    //INFO("num=%d  led_brightness=%d  b=%0.3f\n", num, led_brightness, b);

    x->red   = nearbyint(((rgb >>  0) & 0xff) * b);
    x->green = nearbyint(((rgb >>  8) & 0xff) * b);
    x->blue  = nearbyint(((rgb >> 16) & 0xff) * b);
}

void leds_set_all(unsigned int rgb, int led_brightness)
{
    // xxx set the first and replicate the rest
    for (int num = 0; num < MAX_LED; num++) {
        leds_set(num, rgb, led_brightness);
    }
}

void leds_set_all_off(void)
{
    memset(tx, 0, tx_buff_size);
}

void leds_rotate(int mode)
{
    struct led_s x;

    switch (mode) {
    case 0:  // counterclockwise on respeaker
        x = tx->led[0];
        memmove(&tx->led[0], &tx->led[1], (MAX_LED-1)*sizeof(struct led_s));
        tx->led[MAX_LED-1] = x;
        break;
    case 1:  // clockwise on respeaker
        x = tx->led[MAX_LED-1];
        memmove(&tx->led[1], &tx->led[0], (MAX_LED-1)*sizeof(struct led_s));
        tx->led[0] = x;
        break;
    default:
        ERROR("invalid mode %d\n", mode);
        break;
    }
}

void leds_show(int all_brightness)
{
    int rc;

    if (all_brightness < 0 || all_brightness > 31) {
        ERROR("invalid arg all_brightnesss=%d\n", all_brightness);
        return;
    }

    for (int num = 0; num < MAX_LED; num++) {
        struct led_s *x = &tx->led[num];
        x->start_and_brightness = 0xe0 | all_brightness;
    }

    rc = write(fd, tx, tx_buff_size);
    if (rc != tx_buff_size) {
        ERROR("leds_show_leds write rc=%d exp=%d, %s\n", rc, tx_buff_size, strerror(errno));
        return;
    }
}
