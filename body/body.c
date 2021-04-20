#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <body.h>
#include <misc.h>

#include <gpio.h>
#include <timer.h>
#include <mc.h>
#include <encoder.h>
#include <proximity.h>
#include <button.h>
#include <current.h>
#include <oled.h>
#include <env.h>
#include <imu.h>

int main(int argc, char **argv)
{
    // init devices
    // XXX check rc

    gpio_init();
    timer_init();
    mc_init(2, LEFT_MOTOR, RIGHT_MOTOR);
    encoder_init(2, ENCODER_GPIO_LEFT_B, ENCODER_GPIO_LEFT_A,
                    ENCODER_GPIO_RIGHT_B, ENCODER_GPIO_RIGHT_A);
    proximity_init(2, PROXIMITY_FRONT_GPIO_SIG, PROXIMITY_FRONT_GPIO_ENABLE,
                    PROXIMITY_REAR_GPIO_SIG,  PROXIMITY_REAR_GPIO_ENABLE);
    button_init(2, BUTTON_LEFT, BUTTON_RIGHT);
    current_init(1, CURRENT_ADC_CHAN);
    oled_init(1, 0);
    env_init(0);
    imu_init(0);

    INFO("init devices complete\n");

    pause();

    return 0;
}


