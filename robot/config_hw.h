#ifndef __CONFIG_HW_H__
#define __CONFIG_HW_H__

#ifdef __cplusplus
extern "C" {
#endif

// motor ctlrs
#define LEFT_MOTOR                   "/dev/ttyACM1"       // USB serial device
#define RIGHT_MOTOR                  "/dev/ttyACM0"       // USB serial device

// motor encoders
#define ENCODER_GPIO_LEFT_A          22                   // GPIO IN
#define ENCODER_GPIO_LEFT_B          23                   // GPIO IN
#define ENCODER_GPIO_RIGHT_A         24                   // GPIO IN
#define ENCODER_GPIO_RIGHT_B         25                   // GPIO IN

// proximity sensors
#define PROXIMITY_FRONT_GPIO_ENABLE  26                   // GPIO OUT
#define PROXIMITY_FRONT_GPIO_SIG     27                   // GPIO IN
#define PROXIMITY_REAR_GPIO_ENABLE   16                   // GPIO OUT
#define PROXIMITY_REAR_GPIO_SIG      17                   // GPIO IN

// buttons
#define BUTTON_LEFT                  18                   // GPIO_IN
#define BUTTON_RIGHT                 19                   // GPIO_IN

// current sensor
#define CURRENT_ADC_CHAN             6                    // ADC

#ifdef __cplusplus
}
#endif

#endif
