#include <stdbool.h>
#include <stdarg.h>

#include <config_hw.h>
#include <relay.h>
#include <gpio.h>
#include <misc.h>

int relay_init(void)
{
    int pin;

    for (pin = 0; pin < 32; pin++) {
        if (IS_BIT_SET(RELAY_GPIO_PINS, pin)) {
            set_gpio_func(pin, FUNC_OUT);
        }
    }

    return 0;
}

void relay_ctrl(int pin, bool enable)
{
    if (!IS_BIT_SET(RELAY_GPIO_PINS, pin)) {
        FATAL("relay pin %d invalid\n", pin);
    }

    gpio_write(pin, enable);
}

