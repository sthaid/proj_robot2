#include "MCP9808_temp.h"
#include "../i2c/i2c.h"

#define MCP9808_DEFAULT_ADDR    0x18

#define MCP9808_REG_TEMPERATURE 5
#define MCP9808_CRIT_LIMIT      0x8000
#define MCP9808_UPPER_LIMIT     0x4000
#define MCP9808_LOWER_LIMIT     0x2000
#define MCP9808_SIGN            0x1000

static int dev_addr;

// -----------------  C LANGUAGE API  -----------------------------------

int MCP9808_temp_init(int dev_addr_arg)
{
    // set dev_addr
    dev_addr = (dev_addr_arg == 0 ? MCP9808_DEFAULT_ADDR : dev_addr_arg);

    // init i2c
    if (i2c_init() < 0) {
        return -1;
    }

    return 0;
}

int MCP9808_temp_read(double *degc)
{
    int      rc;
    uint8_t  data[2];
    uint32_t val;

    rc = i2c_read(dev_addr, MCP9808_REG_TEMPERATURE, data, 2);
    if (rc < 0) {
        *degc = 0;
        return -1;
    }

    val = (data[0] << 8) | data[1];
    *degc = (val & 0xfff) / 16.;
    if (val & MCP9808_SIGN) {
        *degc = -(*degc);
    }

    return 0;
}

// -----------------  C LANGUAGE TEST PROGRAM  ---------------------------

#ifdef TEST

// XXX not tested

#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    double temperature;

    if (MCP9808_temp_init(0) < 0) {
        printf("MCP9808_temp_init failed\n");
        return 1;
    }

    while (true) {
        MCP9808_temp_read(&temperature);
        printf("Temperature: %0.1f\n", temperature);
        sleep(1);
    }

    return 0;
}

#endif

