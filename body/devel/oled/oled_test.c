#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>

#include <oled.h>

#define OLED_ID 0

int main(int argc, char **argv)
{
    char str[20];

    if (oled_init(1, 0) < 0) {
        printf("oled_init failed\n");
        return 1;
    }

    for (int i = 0; i < 10; i++) {
        sprintf(str, "HELLO-%d", i);
        oled_draw_str(i, str);
        sleep(1);
    }

    printf("done\n");
    return 0;
}
