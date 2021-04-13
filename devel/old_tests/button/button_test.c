#include <stdio.h>
#include <unistd.h>

#include <body.h>
#include <button.h>

#define MAX_BUTTON 2

void alert(void);

int main(int argc, char **argv)
{
    if (button_init(MAX_BUTTON, BUTTON_LEFT, BUTTON_RIGHT) < 0) {
        printf("button_init failed\n");
        return 1;
    }

    while (1) {
        for (int id = 0; id < MAX_BUTTON; id++) {
            if (button_pressed(id)) {
                printf("Button %d pressed\n", id);
                for (int i = 0; i < id+1; i++) {
                    alert();
                }
            }
        }

        usleep(100000);  // 100 ms
    }

    return 0;
}

void alert(void)
{
    printf("\a");
    fflush(stdout);
    usleep(200000);  // 100 ms
}
