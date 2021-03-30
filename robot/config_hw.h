#ifndef __CONFIG_HW_H__
#define __CONFIG_HW_H__

#ifdef __cplusplus
extern "C" {
#endif

#define RELAY_TEST                   26                   // GPIO OUT
#define RELAY_GPIO_PINS              ((1<<RELAY_TEST))

#define ENCODER0_GPIO_A              16                   // GPIO IN
#define ENCODER0_GPIO_B              17                   // GPIO IN

#define PROXIMITY_FRONT_GPIO_SIG     5                    // GPIO IN
#define PROXIMITY_FRONT_GPIO_ENABLE  6                    // GPIO OUT

#define PROXIMITY_REAR_GPIO_SIG      18                   // GPIO IN
#define PROXIMITY_REAR_GPIO_ENABLE   19                   // GPIO OUT

#define CURRENT_ADC_CHAN             0                    // ADC


#ifdef __cplusplus
}
#endif

#endif
