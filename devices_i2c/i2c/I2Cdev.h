#ifndef __I2CDEV_H__
#define __I2CDEV_H__

#include <stdint.h>

class I2Cdev {
    public:
        I2Cdev();
        
        static bool readBit(uint8_t devAddr, uint8_t regAddr, uint8_t bitNum, uint8_t *data);
        static bool readBitW(uint8_t devAddr, uint8_t regAddr, uint8_t bitNum, uint16_t *data);
        static bool readBits(uint8_t devAddr, uint8_t regAddr, uint8_t bitStart, uint8_t length, uint8_t *data);
        static bool readBitsW(uint8_t devAddr, uint8_t regAddr, uint8_t bitStart, uint8_t length, uint16_t *data);
        static bool readByte(uint8_t devAddr, uint8_t regAddr, uint8_t *data);
        static bool readWord(uint8_t devAddr, uint8_t regAddr, uint16_t *data);
        static bool readBytes(uint8_t devAddr, uint8_t regAddr, uint8_t length, uint8_t *data);
        static bool readWords(uint8_t devAddr, uint8_t regAddr, uint8_t length, uint16_t *data);

        static bool writeBit(uint8_t devAddr, uint8_t regAddr, uint8_t bitNum, uint8_t data);
        static bool writeBitW(uint8_t devAddr, uint8_t regAddr, uint8_t bitNum, uint16_t data);
        static bool writeBits(uint8_t devAddr, uint8_t regAddr, uint8_t bitStart, uint8_t length, uint8_t data);
        static bool writeBitsW(uint8_t devAddr, uint8_t regAddr, uint8_t bitStart, uint8_t length, uint16_t data);
        static bool writeByte(uint8_t devAddr, uint8_t regAddr, uint8_t data);
        static bool writeWord(uint8_t devAddr, uint8_t regAddr, uint16_t data);
        static bool writeBytes(uint8_t devAddr, uint8_t regAddr, uint8_t length, uint8_t *data);
        static bool writeWords(uint8_t devAddr, uint8_t regAddr, uint8_t length, uint16_t *data);
};

#endif
