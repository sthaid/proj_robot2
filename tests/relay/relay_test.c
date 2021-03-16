#include <stdio.h>
#include <stdbool.h>

#include <relay.h>
#include <gpio.h>

#define RELAY5 5

int main(int argc, char **argv)
{
    int rc;

    rc = gpio_init();
    if (rc < 0) return 1;
    rc = relay_init(1, RELAY5);
    if (rc < 0) return 1;

    while (true) {
        printf("enabling relay\n");
        relay_ctrl(RELAY5, true);
        sleep(1);

        printf("disabling relay\n");
        relay_ctrl(RELAY5, false);
        sleep(1);
    }

    return 0;
}
