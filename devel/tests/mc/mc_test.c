#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>

#include <body.h>
#include <mc.h>

void *get_status_thread(void *cx);

int main(int argc, char **argv)
{
    pthread_t tid;

    printf("call mc_init\n");
    if (mc_init(2, LEFT_MOTOR, RIGHT_MOTOR) < 0) {
        printf("FATAL: mc_init failed\n");
        return 1;
    }

    pthread_create(&tid, NULL, get_status_thread, NULL);

    printf("\n\nTEST1: enable, speed0/1=2000, sleep1, emer-stop, sleep5\n");
    mc_enable_all();
    mc_set_speed(0, 2000);
    mc_set_speed(1, 2000);
    sleep(1);
    mc_emergency_stop_all("test1");
    sleep(5);

    printf("\n\nTEST2: speed0/1=1500, sleep3, emer-stop, sleep5\n");
    mc_set_speed_all(1500, 1500);
    sleep(3);
    mc_emergency_stop_all("test2");
    sleep(5);

    printf("\n\nTEST3: speed0/1=1500, sleep10, emer-stop, sleep5\n");
    mc_enable_all();
    mc_set_speed_all(1500, 1500);
    sleep(10);
    mc_emergency_stop_all("test3");
    sleep(5);

    printf("\n\npause to allow time to check current\n");
    pause();
}

void *get_status_thread(void *cx)
{
    mc_status_t *x;

    while (true) {
        x = mc_get_status();
        printf("GET_STATUS: %s %s v=%0.2f current=%0.2f\n",
               MC_STATE_STR(x->state), x->reason_str, x->voltage, x->current);
        sleep(1);
    }

    return NULL;
}
