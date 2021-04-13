#include <string.h>

#include "BME680_tphg.h"
#include "bme680/bme680.h"
#include "../i2c/i2c.h"
#include "misc.h"

#define BME680_DEFAULT_ADDR  0x76

static struct bme680_dev dev;
static int dev_addr;

static int8_t bme680_i2c_read(uint8_t addr, uint8_t reg_addr, uint8_t * reg_data, uint16_t len);
static int8_t bme680_i2c_write(uint8_t addr, uint8_t reg_addr, uint8_t * reg_data, uint16_t len);
static void bme680_delay_ms(uint32_t ms);

// -----------------  C LANGUAGE API  -----------------------------------

int BME680_tphg_init(int dev_addr_arg)
{
    int rc;

    // set dev_addr
    dev_addr = (dev_addr_arg == 0 ? BME680_DEFAULT_ADDR : dev_addr_arg);

    // init i2c
    if (i2c_init() < 0) {
        return -1;
    }

    // init the bme680.cpp code, and provide the i2c access and ms delay routines
    dev.dev_id   = dev_addr;
    dev.intf     = BME680_I2C_INTF;
    dev.read     = bme680_i2c_read;
    dev.write    = bme680_i2c_write;
    dev.delay_ms = bme680_delay_ms;
    rc = bme680_init(&dev);
    if (rc != 0) {
        ERROR("bme680_init rc=%d\n", rc);
        return -1;
    }

    // reset bme680
    rc = bme680_soft_reset(&dev);
    if (rc != 0) {
        ERROR("bme680_soft_reset rc=%d\n", rc);
        return -1;
    }

    // apply bme680 settings
    // - the oversampling and filter settings are copied from
    //   grove_temperature_humidity_bme680.py
    // - gas measurement is disabled; the grove_temperature_humidity_bme680.py
    //   has gas measurement working, but I was not able to get a stable reading
    dev.tph_sett.os_hum  = BME680_OS_2X;
    dev.tph_sett.os_pres = BME680_OS_4X;
    dev.tph_sett.os_temp = BME680_OS_8X;
    dev.tph_sett.filter  = BME680_FILTER_SIZE_3;
#if 1
    dev.gas_sett.run_gas = BME680_DISABLE_GAS_MEAS;
#else
    dev.gas_sett.run_gas = BME680_ENABLE_GAS_MEAS;
    dev.gas_sett.heatr_dur  = 100;
    dev.gas_sett.heatr_temp = 300;
#endif
    rc = bme680_set_sensor_settings(
            (BME680_OST_SEL | BME680_OSH_SEL | BME680_OSP_SEL |
             BME680_FILTER_SEL | BME680_GAS_SENSOR_SEL),
            &dev);
    if (rc != 0) {
        ERROR("bme680_set_sensor_settings rc=%d\n", rc);
        return -1;
    }

#if 0
    // print the profile duration, it should be short (43 ms) when gas measure is disabled
    uint16_t ms;
    bme680_get_profile_dur(&ms, &dev);
    INFO("profile duration %d ms\n", ms);
#endif

    return 0;
}

int BME680_tphg_read(double *temperature, double *pressure, double *humidity, double *gas_resistance)
{
    int rc;
    struct bme680_field_data sensor_data;
    double dummy;

    // xxx
    if (!temperature) temperature = &dummy;
    if (!humidity) humidity = &dummy;
    if (!pressure) pressure = &dummy;
    if (!gas_resistance) gas_resistance = &dummy;

    // xxx
    *temperature = 0;
    *pressure = 0;
    *humidity = 0;
    *gas_resistance = 0;

    // request bme680 start to obtain the temperature, humidity and pressure;
    // note - the gas_resistance is not currently working, just pass in NULL for gas_resistance
    dev.power_mode = BME680_FORCED_MODE;
    rc = bme680_set_sensor_mode(&dev);
    if (rc != 0) {
        ERROR("ERROR bme680_set_sensor_mode rc=%d\n", rc);
        return -1;
    }

    // retrieve the sensor_data from the be680
    memset(&sensor_data,0,sizeof(sensor_data));
    rc = bme680_get_sensor_data(&sensor_data, &dev);
    if (rc != 0) {
        ERROR("bme680_get_sensor_data rc=%d\n", rc);
        return -1;
    }
    
    // return the data to caller
    *temperature = sensor_data.temperature / 100.;
    *humidity = sensor_data.humidity / 1000.;
    *pressure = sensor_data.pressure;
    *gas_resistance = ((sensor_data.status & BME680_HEAT_STAB_MSK) 
                       ? sensor_data.gas_resistance : 0);

    // success
    return 0;
}

static int8_t bme680_i2c_read(uint8_t dev_addr, uint8_t reg_addr, uint8_t * reg_data, uint16_t len)
{
    return i2c_read(dev_addr, reg_addr, reg_data, len);
}

static int8_t bme680_i2c_write(uint8_t dev_addr, uint8_t reg_addr, uint8_t * reg_data, uint16_t len)
{
    return i2c_write(dev_addr, reg_addr, reg_data, len);
}

static void bme680_delay_ms(uint32_t ms)
{
    i2c_delay_ns(ms*1000000);
}

// -----------------  C LANGUAGE TEST PROGRAM  ---------------------------

#ifdef TEST

#include <stdio.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    double temperature, pressure, humidity;

    if (BME680_tphg_init(0) < 0) {
        printf("BME680_tphg_init failed\n");
        return 1;
    }

    while (true) {
        BME680_tphg_read(&temperature, &pressure, &humidity, NULL);
        printf("%0.1f C   %0.0f Pa   %0.2f inch-Hg   %0.1f %%\n", 
                temperature, 
                pressure, pressure/3386,
                humidity);
        sleep(1);
    }

    return 0;
}

#endif
