#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>

#include <config_hw.h>
#include <relay.h>

int main(int argc, char **argv)
{
    int rc;

    printf("be sure to test this code with a GPIO pin that is okay for output\n");
    exit(1);

    rc = relay_init(1, PROXIMITY_FRONT_GPIO_ENABLE);
    if (rc < 0) return 1;

    while (true) {
        printf("enabling relay\n");
        relay_ctrl(0, true);
        sleep(5);

        printf("disabling relay\n");
        relay_ctrl(0, false);
        sleep(7);
    }

    return 0;
}
