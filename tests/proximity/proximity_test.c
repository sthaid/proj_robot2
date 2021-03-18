#include <stdio.h>

#include <gpio.h>
#include <timer.h>
#include <proximity.h>
#include <misc.h>

int main(int argc, char **argv)
{
    int id, sum, poll_rate;
    bool sense;

    if (gpio_init(true) < 0) exit(1);
    if (timer_init() < 0) exit(1);
    if (proximity_init() < 0) exit(1);

    while (true) {
        sleep(1);
        for (id = 0; id < MAX_PROXIMITY; id++) {
            sense = proximity_check(id, &sum, &poll_rate);
            printf("ID %d   SENSOR %d (%d)   POLL_RATE %d /sec\n", 
                  id, sense, sum, poll_rate);
        }
    }

    return 0;
}
