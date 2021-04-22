#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>

#include <body.h>
#include <button.h>

#define MAX_BUTTON 2

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
                int state;
                state = button_get_current_state(id);
                printf("%10s ", BUTTON_STATE_STR(state));
                if (state == BUTTON_STATE_PRESSED) {
                    alert = true;
                }
            }
            printf("%c\n", (alert ? '\a' : ' '));
            usleep(100000);  // 100 ms
        }
    } else if (test == 2) {
        // process all button events
        while (true) {
            while (true) {
                button_event_t ev;
                int rc = button_get_event(&ev);
                if (rc == -1) {
                    break;
                }
                printf("%d  %s", ev.id, BUTTON_STATE_STR(ev.state));
                if (ev.state == BUTTON_STATE_RELEASED) {
                    printf("  %5.3f secs", ev.pressed_duration_us/1000000.);
                }
                printf("\a\n");
            }
            usleep(100000);  // 100 ms
        }
    } else {
        printf("ERROR: test %d invalid\n", test);
    }

    return 0;
}
