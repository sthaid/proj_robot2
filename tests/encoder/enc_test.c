#include <stdio.h>

#include <gpio.h>
#include <timer.h>
#include <encoder.h>
#include <misc.h>

int main(int argc, char **argv)
{
    int id;
    int position, speed, errors, poll_rate;

    if (gpio_init(true) < 0) exit(1);
    if (timer_init() < 0) exit(1);
    if (encoder_init() < 0) exit(1);

    while (true) {
        sleep(1);
        for (id = 0; id < MAX_ENCODER; id++) {
            encoder_get_ex(0, &position, &speed, &errors, &poll_rate);
            printf("ID %d : POS %d   SPEED %d   ERRORS %d  POLL_RATE %d\n", 
                   id, position, speed, errors, poll_rate);
        }
        printf("\n");
    }

    return 0;
}
