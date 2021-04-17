#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>

#include <oled.h>

#define OLED_ID 0

int main(int argc, char **argv)
{
    if (oled_init(1, 0) < 0) {
        printf("oled_init failed\n");
        return 1;
    }

    oled_set_intvl_us(OLED_ID, 0);

    oled_set_str(OLED_ID, 0, "hello");
    oled_set_str(OLED_ID, 1, "world");
    oled_set_str(OLED_ID, 4, "44444");
    oled_set_str(OLED_ID, 7, "123456");

    while (true) {
        usleep(3000000);
        oled_set_next(OLED_ID);
    }

    return 0;
}
