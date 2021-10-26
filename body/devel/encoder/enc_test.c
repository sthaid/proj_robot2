#include <stdio.h>
#include <unistd.h>

#include <body.h>
#include <encoder.h>

int main(int argc, char **argv)
{
    int id, count=0;

    // init encoders
    if (encoder_init(2, ENCODER_GPIO_LEFT_B, ENCODER_GPIO_LEFT_A,
                        ENCODER_GPIO_RIGHT_B, ENCODER_GPIO_RIGHT_A))
    {
        printf("encoder_init failed\n");
        return 1;
    }

    // sleep 10 secs
    printf("sleep for 10 secs\n");
    sleep(10);

    // enable encoders
    printf("enable encoders\n");
    for (id = 0; id < 2; id++) {
        encoder_enable(id);
    }

    // read encoder values once per sec
    printf("read encoder values once per sec\n"); 
    while (1) {
        // every 10 secs get the poll intvl
        if ((++count % 10) == 0) {
            int poll_intvl_us;
            poll_intvl_us = encoder_get_poll_intvl_us();
            printf("POLL_INTVL_US = %d\n", poll_intvl_us);
            printf("\n");
        }

        // read both encoders, and print their values
        for (id = 0; id < 2; id++) {
            printf("ID %d : COUNT %d   SPEED %d   ERRORS %d\n", 
                   id,
                   encoder_get_count(id),
                   encoder_get_speed(id),
                   encoder_get_errors(id));
        }
        printf("\n");

        // sleep 1 sec
        sleep(1);
    }

    return 0;
}
