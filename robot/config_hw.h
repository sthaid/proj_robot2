#ifndef __CONFIG_HW_H__
#define __CONFIG_HW_H__

#ifdef __cplusplus
extern "C" {
#endif

#define RELAY_TEST                   5
#define RELAY_GPIO_PINS              ((1<<RELAY_TEST))

#define ENCODER0_GPIO_A              16
#define ENCODER0_GPIO_B              17

#define PROXIMITY_FRONT_GPIO_SIG     18
#define PROXIMITY_FRONT_GPIO_ENABLE  19

#define CURRENT_ADC_CHAN             0

#ifdef __cplusplus
}
#endif

#endif
