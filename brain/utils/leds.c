#include <utils.h>

#include <linux/spi/spidev.h>
#include <wiringPi.h>

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
    // this enables Vcc for the Respeaker LEDs
    if (wiringPiSetupGpio() == -1) {
        FATAL("wiringPiSetupGpio failed\n");
    }
    pinMode (5, OUTPUT);
    digitalWrite(5, 1);

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
    leds_stage_all(LED_OFF,0);
    leds_commit();
}

// -----------------  XXX  ---------------------------------------------------------------

void leds_stage_led(int num, unsigned int rgb, int led_brightness)
{
    struct led_s *x = &tx->led[num];

    static double b[101];
    static bool first_call = true;

    if (first_call) {
        first_call = false;
        for (int i = 0; i <= 100; i++) {
            b[i] = 1e-6 * (i * i * i) + .002;
            if (b[i] > 1) b[i] = 1;
        }
    }

    if (num < 0 || num >= MAX_LED) {
        ERROR("invalid arg num=%d\n", num);
        return;
    }

    if (led_brightness < 0 || led_brightness > 100) {
        ERROR("invalid arg led_brightnesss=%d\n", led_brightness);
        return;
    }

    x->red   = nearbyint(((rgb >>  0) & 0xff) * b[led_brightness]);
    x->green = nearbyint(((rgb >>  8) & 0xff) * b[led_brightness]);
    x->blue  = nearbyint(((rgb >> 16) & 0xff) * b[led_brightness]);
}

void leds_stage_all(unsigned int rgb, int led_brightness)
{
    if (rgb == LED_OFF) {
        memset(tx, 0, tx_buff_size);
    } else {
        leds_stage_led(0, rgb, led_brightness);
        for (int num = 1; num < MAX_LED; num++) {
            tx->led[num] = tx->led[0];
        }
    }
}

void leds_stage_rotate(int mode)
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

void leds_commit(void)
{
    int rc;
    int all_brightness = 31;

// xxx if all off use 0

    for (int num = 0; num < MAX_LED; num++) {
        struct led_s *x = &tx->led[num];
        x->start_and_brightness = 0xe0 | all_brightness;
    }

    rc = write(fd, tx, tx_buff_size);
    if (rc != tx_buff_size) {
        ERROR("leds_commit write rc=%d exp=%d, %s\n", rc, tx_buff_size, strerror(errno));
        return;
    }
}
