#include <stdbool.h>
#include <stdarg.h>

#include <relay.h>
#include <gpio.h>
#include <misc.h>

static bool relay_gpionum_valid[32];

int relay_init(int num, ...)
{
    va_list ap;
    int i, ret = 0;

    va_start(ap, num);
    for (i = 0; i < num; i++) {
        int relay_gpionum = va_arg(ap, int);
        if (relay_gpionum < 0 || relay_gpionum >= 32) {
            ERROR("relay_gpionum %d invalid\n", relay_gpionum);
            ret = -1;
            break;
        }
        set_gpio_func(relay_gpionum, FUNC_OUT);
        relay_gpionum_valid[relay_gpionum] = true;
    }
    va_end(ap);

    return ret;
}

void relay_ctrl(int relay_gpionum, bool enable)
{
    if (!relay_gpionum_valid[relay_gpionum]) {
        FATAL("relay_gpionum %d invalid\n", relay_gpionum);
    }

    gpio_write(relay_gpionum, enable);
}

