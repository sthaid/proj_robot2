#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <math.h>

#include <body.h>
#include <gpio.h>
#include <timer.h>
#include <encoder.h>
#include <mc.h>

void *get_status_thread(void *cx);

int main(int argc, char **argv)
{
    pthread_t tid;
    char s[100];
    int speed;

    while (true) {
        signed char opt_char = getopt(argc, argv, "d");
        if (opt_char == -1) {
            break;
        }
        switch (opt_char) {
        case 'd':
            printf("enabling debug mode\n");
            mc_debug_mode(true);
            break;
        default:
            printf("XXX\n");
            return -1;
        }
    }

    if (gpio_init() < 0) {
        printf("FATAL: gpio_init failed\n");
        return 1;
    }
    if (timer_init() < 0) {
        printf("FATAL: timer_init failed\n");
        return 1;
    }
    if (mc_init(2, LEFT_MOTOR, RIGHT_MOTOR) < 0) {
        printf("FATAL: mc_init failed\n");
        return 1;
    }
    if (encoder_init(2, ENCODER_GPIO_LEFT_B, ENCODER_GPIO_LEFT_A,
                        ENCODER_GPIO_RIGHT_B, ENCODER_GPIO_RIGHT_A) < 0) {
        printf("FATAL: encoder_init failed\n");
        return 1;
    }

    encoder_enable(0);
    encoder_enable(1);

    pthread_create(&tid, NULL, get_status_thread, NULL);

    while (true) {
        printf("Enter speed: ");
        if (fgets(s, sizeof(s), stdin) == NULL) break;

        if (sscanf(s, "%d", &speed) != 1 || abs(speed) > 3200) {
            printf("ERROR: speed invalid\n");
            continue;
        }

        mc_enable_all();
        mc_set_speed(0, speed);
        mc_set_speed(1, speed);
    }

    mc_disable_all();
    return 0;
}

void *get_status_thread(void *cx)
{
    mc_status_t *status;

    while (true) {
        status = mc_get_status();
        if (status->debug_mode_enabled) {
            struct debug_mode_mtr_vars_s *mr0 = &status->debug_mode_mtr_vars[0]; 
            struct debug_mode_mtr_vars_s *mr1 = &status->debug_mode_mtr_vars[1]; 
            printf("STATUS: %s V=%-5.2f A=%-4.2f TGT_SPEED=%d,%d\n"
                   "        %d: errstat=0x%04x tgt=%-4d curr=%-4d max_accel=%-2d max_decel=%-2d v=%5.2f A=%4.2f\n"
                   "        %d: errstat=0x%04x tgt=%-4d curr=%-4d max_accel=%-2d max_decel=%-2d v=%5.2f A=%4.2f\n",
                   MC_STATE_STR(status->state), status->voltage, status->motors_current,
                     status->target_speed[0], status->target_speed[1],
                   0, mr0->error_status, mr0->target_speed, mr0->current_speed, mr0->max_accel, mr0->max_decel, 
                      mr0->input_voltage/1000., mr0->current/1000.,
                   1, mr1->error_status, mr1->target_speed, mr1->current_speed, mr1->max_accel, mr1->max_decel, 
                      mr1->input_voltage/1000., mr1->current/1000.);
        } else {
            printf("STATUS: %s V=%-5.2f A=%-4.2f TGT_SPEED=%d,%d\n",
                   MC_STATE_STR(status->state), status->voltage, status->motors_current,
                     status->target_speed[0], status->target_speed[1]);
        }

        printf("ENCODER SPEED  %0.3f  %0.3f mph\n\n",
               ENC_COUNT_TO_FEET(encoder_get_speed(0)) * 0.681818,
               ENC_COUNT_TO_FEET(encoder_get_speed(1)) * 0.681818);

        sleep(1);
    }

    return NULL;
}
