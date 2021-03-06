#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>

#include <env.h>
#include <BMP280_tp.h>
#include <misc.h>

static double temperature;
static double pressure;

static void * env_thread(void *cx);

// -----------------  API  --------------------------------------

int env_init(int dev_addr)  // multiple instances not supported
{
    static pthread_t tid;

    // check if already initialized
    if (tid) {
        ERROR("already initialized\n");
        return -1;
    }

    // init BMP280 i2c device
    if (BMP280_tp_init(dev_addr) < 0) {
        ERROR("BMP280_tp_init failed\n");
        return -1;
    }

    // create thread to read the temperature and pressure once per sec
    pthread_create(&tid, NULL, env_thread, NULL);

    // return success
    return 0;
}

double env_get_temperature_degc(void)
{
    return temperature;
}

double env_get_pressure_pascal(void)
{
    return pressure;
}

double env_get_temperature_degf(void)
{
    return CENTIGRADE_TO_FAHRENHEIT(temperature);
}

double env_get_pressure_inhg(void)
{
    return PASCAL_TO_INHG(pressure);
}

// -----------------  THREAD-------------------------------------

static void * env_thread(void *cx)
{
    while (true) {
        BMP280_tp_read(&temperature, &pressure);
        usleep(1000000);   // 1 sec
    }

    return NULL;
}

