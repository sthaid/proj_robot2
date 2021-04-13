#include "BMP280_tp.h"
#include "bmp280/BMP280.h"
#include "../i2c/i2c.h"

#define BMP280_TP_DEFAULT_ADDR   0x77

class BMP280 *bmp280;

extern "C" {

// -----------------  C LANGUAGE API  -----------------------------------

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

// -----------------  C LANGUAGE TEST PROGRAM  ---------------------------

#ifdef TEST

#include <stdio.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    double temperature, pressure;

    if (BMP280_tp_init(0) < 0) {
        printf("BMP280_tp_init failed\n");
        return 1;
    }

    while (true) {
        BMP280_tp_read(&temperature, &pressure);
        printf("%0.1f C   %0.0f Pa   %0.2f inch-Hg\n",
           temperature,
           pressure, pressure/3386);
        sleep(1);
    }

    return 0;
}

#endif
