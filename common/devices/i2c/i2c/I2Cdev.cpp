#include <stdio.h>
#include <stdlib.h>

#include "I2Cdev.h"
#include "i2c.h"

#define NOT_IMPLEMENTED \
    do { \
        printf("%s NOT_IMPLEMENTED\n", __func__); \
        exit(1); \
    } while (0)

I2Cdev::I2Cdev() {}

// NOTE: only the member functions being used are coded
        
// -----------------  READ MEMBER FUNCTIONS  -------------------

bool I2Cdev::readBit(uint8_t devAddr, uint8_t regAddr, uint8_t bitNum, uint8_t *data)
{
    NOT_IMPLEMENTED;
}

bool I2Cdev::readBitW(uint8_t devAddr, uint8_t regAddr, uint8_t bitNum, uint16_t *data)
{
    NOT_IMPLEMENTED;
}

bool I2Cdev::readBits(uint8_t devAddr, uint8_t regAddr, uint8_t bitStart, uint8_t length, uint8_t *data)
{
    NOT_IMPLEMENTED;
}

bool I2Cdev::readBitsW(uint8_t devAddr, uint8_t regAddr, uint8_t bitStart, uint8_t length, uint16_t *data)
{
    NOT_IMPLEMENTED;
}

bool I2Cdev::readByte(uint8_t devAddr, uint8_t regAddr, uint8_t *data)
{
    NOT_IMPLEMENTED;
}

bool I2Cdev::readWord(uint8_t devAddr, uint8_t regAddr, uint16_t *data)
{
    NOT_IMPLEMENTED;
}

bool I2Cdev::readBytes(uint8_t devAddr, uint8_t regAddr, uint8_t length, uint8_t *data)
{
    int rc;

    rc = i2c_read(devAddr, regAddr, data, length);
    return rc == 0;
}

bool I2Cdev::readWords(uint8_t devAddr, uint8_t regAddr, uint8_t length, uint16_t *data)
{
    NOT_IMPLEMENTED;
}

// -----------------  WRITE MEMBER FUNCTIONS  ------------------

bool I2Cdev::writeBit(uint8_t devAddr, uint8_t regAddr, uint8_t bitNum, uint8_t data)
{
    uint8_t reg_data;
    int rc;

    i2c_read(devAddr, regAddr, &reg_data, 1);
    reg_data = (data != 0) ? (reg_data | (1 << bitNum))
                           : (reg_data & ~(1 << bitNum));
    rc = i2c_write(devAddr, regAddr, &reg_data, 1);
    return rc == 0;
}

bool I2Cdev::writeBitW(uint8_t devAddr, uint8_t regAddr, uint8_t bitNum, uint16_t data)
{
    NOT_IMPLEMENTED;
}

bool I2Cdev::writeBits(uint8_t devAddr, uint8_t regAddr, uint8_t bitStart, uint8_t length, uint8_t data)
{
    uint8_t mask = ((1 << length) - 1) << (bitStart - length + 1);
    uint8_t reg_data;
    int rc;

    i2c_read(devAddr, regAddr, &reg_data, 1);
    data <<= (bitStart - length + 1);
    data &= mask;
    reg_data &= ~(mask);
    reg_data |= data;
    rc = i2c_write(devAddr, regAddr, &reg_data, 1);
    return rc == 0;
}

bool I2Cdev::writeBitsW(uint8_t devAddr, uint8_t regAddr, uint8_t bitStart, uint8_t length, uint16_t data)
{
    NOT_IMPLEMENTED;
}

bool I2Cdev::writeByte(uint8_t devAddr, uint8_t regAddr, uint8_t data)
{
    int rc;

    rc = i2c_write(devAddr, regAddr, &data, 1);
    return rc == 0;
}

bool I2Cdev::writeWord(uint8_t devAddr, uint8_t regAddr, uint16_t data)
{
    NOT_IMPLEMENTED;
}

bool I2Cdev::writeBytes(uint8_t devAddr, uint8_t regAddr, uint8_t length, uint8_t *data)
{
    NOT_IMPLEMENTED;
}

bool I2Cdev::writeWords(uint8_t devAddr, uint8_t regAddr, uint8_t length, uint16_t *data)
{
    NOT_IMPLEMENTED;
}

