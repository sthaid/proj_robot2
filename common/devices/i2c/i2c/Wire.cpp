#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <mutex>

#include "Wire.h"
#include "i2c.h"

class Wire Wire;
class std::mutex mtx;

// -----------------  INIT  ---------------

void Wire::begin(void) 
{
    i2c_init();

    write_dev_addr = 0;
    write_buff_len = 0;
    read_buff_len = 0;
    read_buff_idx = 0;
    memset(read_buff,0,sizeof(read_buff));
    memset(write_buff,0,sizeof(write_buff));
};

// -----------------  WRITE  --------------

void Wire::beginTransmission(int dev_addr)
{
    mtx.lock();

    if (write_buff_len != 0) {
        printf("%s FATAL write_buff_len = %d\n", __func__, write_buff_len);
        exit(1);
    }

    write_dev_addr = dev_addr;
};

void Wire::write(uint8_t reg)
{
    write_buff[write_buff_len++] = reg;
}

void Wire::endTransmission(void)
{
    if (write_buff_len == 0) {
        printf("%s FATAL write_buff_len = %d\n", __func__, write_buff_len);
        exit(1);
    }

    i2c_write(write_dev_addr, write_buff[0], write_buff+1, write_buff_len-1);
    write_buff_len = 0;

    mtx.unlock();
}

// -----------------  READ  ---------------

void Wire::requestFrom(uint8_t dev_addr, uint8_t len)
{
    mtx.lock();

    if (read_buff_len != 0) {
        printf("%s FATAL read_buff_len = %d\n", __func__, read_buff_len);
        exit(1);
    }

    i2c_read_data(dev_addr, read_buff, len);
    read_buff_len = len;
    read_buff_idx = 0;
}

uint8_t Wire::available(void)
{
    return read_buff_len;
}

uint8_t Wire::read(void)
{
    uint8_t read_ret;

    read_ret = read_buff[read_buff_idx++];

    if (read_buff_idx == read_buff_len) {
        read_buff_idx = 0;
        read_buff_len = 0;
        mtx.unlock();
    }

    return read_ret;
}

