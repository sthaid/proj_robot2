#include "BMP280_tp.h"
#include "bmp280/BMP280.h"
#include "../i2c/i2c.h"

#define BMP280_TP_DEFAULT_ADDR   0x77

class BMP280 *bmp280;

extern "C" {

int BMP280_tp_init(int dev_addr)
{
    // set dev_addr
    // XXX this is not used
    if (dev_addr == 0) {
        dev_addr = BMP280_TP_DEFAULT_ADDR;
    }

    // init i2c
    if (i2c_init() < 0) {
        return -1;
    }

    // create new BMP280, and initialize
    bmp280 = new BMP280 ();
    bmp280->init();

    // success
    return 0;
}

int BMP280_tp_read(double *temperature, double *pressure)
{
    *temperature = bmp280->getTemperature();
    *pressure = bmp280->getPressure();
    return 0;
}

} // extern "C"
