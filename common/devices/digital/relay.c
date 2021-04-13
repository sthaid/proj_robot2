#include <stdbool.h>
#include <stdarg.h>

#include <relay.h>
#include <gpio.h>
#include <misc.h>

static struct info_s {
    int gpio_pin;
} info_tbl[10];
static int max_info;

int relay_init(int max_info_arg, ...)   // int gpio_pin, ...
{
    static bool initialized;
    va_list ap;

    // error if already initialized
    if (initialized) {
        ERROR("already initialized\n");
        return -1;
    }

    // init gpio
    if (gpio_init() < 0) {
        ERROR("gpio_init failed\n");
        return -1;
    }

    // save hardware info
    va_start(ap, max_info_arg);
    for (int i = 0; i < max_info_arg; i++) {
        info_tbl[i].gpio_pin = va_arg(ap, int);
    }
    max_info = max_info_arg;
    va_end(ap);

    // set all relay gpio pins to FUNC_OUT
    for (int i = 0; i < max_info_arg; i++) {
        set_gpio_func(info_tbl[i].gpio_pin, FUNC_OUT);
    }

    // set initialized flag
    initialized = true;

    // return success
    return 0;
}

void relay_ctrl(int id, bool enable)
{
    gpio_write(info_tbl[id].gpio_pin, enable);
}

