#include "MCP9808_temp.h"
#include "../common/i2c.h"

#define MCP9808_DEFAULT_ADDR    0x18

#define MCP9808_REG_TEMPERATURE 5
#define MCP9808_CRIT_LIMIT      0x8000
#define MCP9808_UPPER_LIMIT     0x4000
#define MCP9808_LOWER_LIMIT     0x2000
#define MCP9808_SIGN            0x1000

int MCP9808_temp_init(int dev_addr)
{
    if (dev_addr == 0) {
        dev_addr = MCP9808_DEFAULT_ADDR;
    }

    if (i2c_init() < 0) {
        return -1;
    }

    return 0;
}

int MCP9808_temp_read(int dev_addr, double *degc)
{
    int      rc;
    uint8_t  data[2];
    uint32_t val;

    if (dev_addr == 0) {
        dev_addr = MCP9808_DEFAULT_ADDR;
    }

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
