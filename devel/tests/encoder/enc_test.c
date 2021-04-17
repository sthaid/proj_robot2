#include <stdio.h>
#include <unistd.h>

#include <body.h>
#include <encoder.h>

int main(int argc, char **argv)
{
    int id;
    int position, speed, errors, poll_rate;

    if (encoder_init(2, ENCODER_GPIO_LEFT_B, ENCODER_GPIO_LEFT_A,
                        ENCODER_GPIO_RIGHT_B, ENCODER_GPIO_RIGHT_A))
    {
        printf("encoder_init failed\n");
        return 1;
    }

    while (1) {
        sleep(1);
        for (id = 0; id < 2; id++) {
            encoder_get_ex(0, &position, &speed, &errors, &poll_rate);
            printf("ID %d : POS %d   SPEED %d   ERRORS %d  POLL_RATE %d\n", 
                   id, position, speed, errors, poll_rate);
        }
        printf("\n");
    }

    return 0;
}
