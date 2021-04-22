#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#include <body.h>
#include <mc.h>

void *get_status_thread(void *cx);

int main(int argc, char **argv)
{
    pthread_t tid;
    int opt;

    opt = getopt(argc, argv, "d");
    switch (opt) {
    case 'd':
        printf("enabling debug mode\n");
        mc_debug_mode(true);
        break;
    case -1:
        break;
    default:
        exit(1);
    }

    if (mc_init(2, LEFT_MOTOR, RIGHT_MOTOR) < 0) {
        printf("FATAL: mc_init failed\n");
        return 1;
    }

    pthread_create(&tid, NULL, get_status_thread, NULL);

    printf("\n\n"
           "--------------------------------------------------------------------\n"
           "--- TEST1: enable, speed0/1=2000, sleep1, emer-stop, sleep5\n"
           "--------------------------------------------------------------------\n"
           "\n");
    mc_enable_all();
    mc_set_speed(0, 2000);
    mc_set_speed(1, 2000);
    sleep(1);
    mc_emergency_stop_all("test1");
    sleep(5);

    printf("\n\n"
           "--------------------------------------------------------------------\n"
           "--- TEST2: speed0/1=1500, sleep3, emer-stop, sleep5\n"
           "--------------------------------------------------------------------\n"
           "\n");
    mc_set_speed_all(1500, 1500);
    sleep(3);
    mc_emergency_stop_all("test2");
    sleep(5);

    printf("\n\n"
           "--------------------------------------------------------------------\n"
           "--- TEST3: speed0/1=1500, sleep10, emer-stop, sleep5\n"
           "--------------------------------------------------------------------\n"
           "\n");
    mc_enable_all();
    mc_set_speed_all(1500, 1500);
    sleep(10);
    mc_emergency_stop_all("test3");
    sleep(5);

    printf("\n\n"
           "--------------------------------------------------------------------\n"
           "--- PAUSE to allow time to check current\n"
           "--------------------------------------------------------------------\n"
           "\n");
    pause();
}

void *get_status_thread(void *cx)
{
    mc_status_t *status;

    while (true) {
        status = mc_get_status();
        if (status->debug_mode_enabled) {
            struct debug_mode_mtr_vars_s *mr0 = &status->debug_mode_mtr_vars[0]; 
            struct debug_mode_mtr_vars_s *mr1 = &status->debug_mode_mtr_vars[1]; 
            printf("STATUS: %s %s V=%-5.2f A=%-4.2f TGT_SPEED=%d,%d\n"
                   "        %d: errstat=0x%04x tgt=%-4d curr=%-4d max_accel=%-2d max_decel=%-2d v=%5.2f A=%4.2f\n"
                   "        %d: errstat=0x%04x tgt=%-4d curr=%-4d max_accel=%-2d max_decel=%-2d v=%5.2f A=%4.2f\n"
                   "\n",
                   MC_STATE_STR(status->state), status->reason_str, status->voltage, status->motors_current,
                     status->target_speed[0], status->target_speed[1],
                   0, mr0->error_status, mr0->target_speed, mr0->current_speed, mr0->max_accel, mr0->max_decel, 
                      mr0->input_voltage/1000., mr0->current/1000.,
                   1, mr1->error_status, mr1->target_speed, mr1->current_speed, mr1->max_accel, mr1->max_decel, 
                      mr1->input_voltage/1000., mr1->current/1000.);
        } else {
            printf("STATUS: %s %s V=%-5.2f A=%-4.2f TGT_SPEED=%d,%d\n\n",
                   MC_STATE_STR(status->state), status->reason_str, status->voltage, status->motors_current,
                     status->target_speed[0], status->target_speed[1]);
        }
        sleep(1);
    }

    return NULL;
}
