#include <string.h>

#include "SSD1306_oled.h"
#include "../i2c/i2c.h"
#include "../u8g2/u8g2.h"
#include "misc.h"

#define SSD1306_OLED_DEFAULT_ADDR  0x3c

static int dev_addr;
static u8g2_t u8g2;

static uint8_t u8x8_byte_linux_i2c(u8x8_t * u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);
static uint8_t u8x8_linux_i2c_delay(u8x8_t * u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);

// -----------------  C LANGUAGE API  -----------------------------------

int SSD1306_oled_init(int dev_addr_arg)
{
    // set dev_addr
    dev_addr = (dev_addr_arg == 0 ? SSD1306_OLED_DEFAULT_ADDR : dev_addr_arg);

    // init i2c
    if (i2c_init() < 0) {
        return -1;
    }

    // call setup constructor
    u8g2_Setup_ssd1306_i2c_128x32_univision_f(
        &u8g2,
        U8G2_R0,
        u8x8_byte_linux_i2c,
        u8x8_linux_i2c_delay);

    // set the i2c address of the display
    u8g2_SetI2CAddress(&u8g2, dev_addr);

    // reset and configure the display
    u8g2_InitDisplay(&u8g2);

    // de-activate power-save; becaue if activated nothing is displayed
    u8g2_SetPowerSave(&u8g2, 0);

    // clears the memory frame buffer
    u8g2_ClearBuffer(&u8g2);

    // defines the font to be used by subsequent drawing instruction
    u8g2_SetFont(&u8g2, u8g2_font_logisoso32_tf);

    // set font reference ascent and descent (was in the sample code, but 0not needed)
    //u8g2_SetFontRefHeightText(&u8g2);

    // set reference position to the top of the font; y=0 is top of font
    u8g2_SetFontPosTop(&u8g2);

    // draw string; setting the initial string to be displayed prior to subsequent
    // calls to ssd1306_oled_u8g2_drawstr
    u8g2_DrawStr(&u8g2, 0, 0, "  ---  ");

    // send the contents of the memory frame buffer to the display
    u8g2_SendBuffer(&u8g2);

    // success
    return 0;
}

int SSD1306_oled_drawstr(char *s)
{
    u8g2_ClearBuffer(&u8g2);
    u8g2_DrawStr(&u8g2, 0, 0, s);
    u8g2_SendBuffer(&u8g2);
    return 0;
}

// return 1 for success, 0 for failure
static uint8_t u8x8_byte_linux_i2c(u8x8_t * u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    static uint8_t data[32];
    static int idx;
    int rc;

    switch (msg) {
    case U8X8_MSG_BYTE_SEND:
        for (int i = 0; i < arg_int && idx < sizeof(data); i++, idx++) {
            data[idx] = *(uint8_t *) (arg_ptr + i);
        }
        break;
    case U8X8_MSG_BYTE_INIT:
        // this is supposed to open(DEVNAME), but we've already done that in 
        // sensor_init call to i2c_init
        break;
    case U8X8_MSG_BYTE_SET_DC:
        // ignored for i2c 
        break;
    case U8X8_MSG_BYTE_START_TRANSFER:
        memset(data, 0, sizeof(data));
        idx = 0;
        break;
    case U8X8_MSG_BYTE_END_TRANSFER:
        rc = i2c_write(u8x8->i2c_address, data[0], data+1, idx-1);
        if (rc < 0) {
            ERROR("i2c_write\n");
            return 0;
        }
        break;
    default:
        ERROR("unknown msg type %d\n", msg);
        return 0;
    }

    return 1;
}

// return 1 for success, 0 for failure
static uint8_t u8x8_linux_i2c_delay(u8x8_t * u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    uint32_t nsec;

    switch (msg) {
    case U8X8_MSG_DELAY_NANO:    // delay arg_int * 1 nano second
        nsec = arg_int;
        break;
    case U8X8_MSG_DELAY_100NANO: // delay arg_int * 100 nano seconds
        nsec = arg_int * 100;
        break;
    case U8X8_MSG_DELAY_10MICRO: // delay arg_int * 10 micro seconds
        nsec = arg_int * 10000;
        break;
    case U8X8_MSG_DELAY_MILLI:   // delay arg_int * 1 milli second
        nsec = arg_int * 1000000;
        break;
    default:
        return 0;
    }

    i2c_delay_ns(nsec);
    return 1;
}

// -----------------  C LANGUAGE TEST PROGRAM  ---------------------------

#ifdef TEST

#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    if (SSD1306_oled_init(0) < 0) {
        printf("SSD1306_oled_init failed\n");
        return 1;
    }

    while (true) {
        SSD1306_oled_drawstr("HELLO");
        sleep(1);
        SSD1306_oled_drawstr("WORLD");
        sleep(1);
    }

    return 0;
}

#endif


