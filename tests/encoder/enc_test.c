#include <stdio.h>

#include <gpio.h>
#include <timer.h>
#include <encoder.h>
#include <misc.h>

int main(int argc, char **argv)
{
    int id;
    int position, speed;
    unsigned int poll_count, poll_count_last=0, errors[16];

    if (gpio_init(true) < 0) exit(1);
    if (timer_init() < 0) exit(1);
    if (encoder_init() < 0) exit(1);

    while (true) {
        sleep(1);
        encoder_get_stats(errors, &poll_count);
        for (id = 0; id < MAX_ENCODER; id++) {
            encoder_get(0, &position, &speed);
            printf("ID %d : POS %d   SPEED %d   ERRORS %d\n", 
                   id, position, speed, errors[id]);
        }
        printf("POLL FREQ %d /sec\n", poll_count-poll_count_last);
        poll_count_last = poll_count;
        printf("\n");
    }

    return 0;
}
