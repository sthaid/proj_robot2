#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>

#include <body.h>
#include <button.h>

#define MAX_BUTTON 2

static void button_cb(int id, bool pressed, int pressed_duration_us);

int main(int argc, char **argv)
{
    int test;

    if ((argc != 2) ||
        (sscanf(argv[1], "%d", &test) != 1) ||
        (test != 1 && test != 2))
    {
        printf("USAGE: button_test <1|2>\n");
        return 1;
    }

    if (button_init(MAX_BUTTON, BUTTON_LEFT, BUTTON_RIGHT) < 0) {
        printf("button_init failed\n");
        return 1;
    }

    if (test == 1) {
        // get current states of all buttons
        while (true) {
            bool alert = false;
            for (int id = 0; id < MAX_BUTTON; id++) {
                int pressed;
                pressed = button_is_pressed(id);
                printf("%10s ", pressed ? "PRESSED" : "RELEASED");
                if (pressed) {
                    alert = true;
                }
            }
            printf("%c\n", (alert ? '\a' : ' '));
            usleep(100000);  // 100 ms
        }
    } else if (test == 2) {
        // register callback for all buttons
        for (int id = 0; id < MAX_BUTTON; id++) {
            button_register_cb(id, button_cb);
        }

        // pause forever
        while (1) pause();
    }

    return 0;
}

static void button_cb(int id, bool pressed, int pressed_duration_us)
{
    if (pressed) {
        printf("%d PRESSED\n", id);
    } else {
        printf("%d RELEASED - duration_us=%d\n", id, pressed_duration_us);
    }
}
