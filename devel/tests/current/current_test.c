#include <stdio.h>
#include <unistd.h>

#include <body.h>
#include <current.h>

int main(int argc, char **argv)
{
    double current;

    if (current_init(1, CURRENT_ADC_CHAN) < 0) {
        printf("current_init failed\n");
        return 1;
    }

    while (1) {
        sleep(1);
        current = current_read_smoothed(0);
        printf("current = %5.2f A\n", current);
    }

    return 0;
}
