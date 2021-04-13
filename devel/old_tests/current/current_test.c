#include <stdio.h>

#include <body.h>
#include <current.h>

int main(int argc, char **argv)
{
    double current;
    double min=100, max=-100;

    if (current_init(1, CURRENT_ADC_CHAN) < 0) {
        printf("current_init failed\n");
        return 1;
    }

    while (1) {
        current_read(0, &current);
        if (current < min) min = current;
        if (current > max) max = current;
        printf("current = %5.2f   range = %5.2f ... %5.2f\n", current, min, max);
    }

    return 0;
}
