#ifndef __BODY_H__
#define __BODY_H__

#ifdef __cplusplus
extern "C" {
#endif

// wheel, motor,  encoder, and ctlr characteristics
//
// note: the MTR_SPEED_TO_MPH and MTR_MPH_TO_SPEED are for unloaded motors,
// the drive_cal_cvt_mph_to_mtr_speeds() should normally be used
//
// motors:
// - https://www.pololu.com/product/4845
// - motor no load performance: 210 RPM at 12V
// - encoder:  2248.86 counts/rev of gearbox output shaft
// motor ctlrs:
// - https: https://www.pololu.com/product/1362   
// - motor speed ctlr full speed:  3200
// wheels:
// - https://www.pololu.com/product/3691
//
#define WHEEL_DIAMETER             (0.080)   // meters
#define MOTOR_RPM_AT_12V           (210.)
#define FEET_PER_REV               ((WHEEL_DIAMETER * M_PI) * 3.28084)    // 0.824 ft
#define ENC_COUNT_TO_FEET(cnt)     ((cnt) * ((1/2248.86) * FEET_PER_REV))
#define ENC_SPEED_TO_MPH(encspd)   ((encspd) * ((1/2248.86) * FEET_PER_REV * 0.681818))
#define MTR_SPEED_TO_MPH(mtrspd)   ((mtrspd) * ((1./3200) * MOTOR_RPM_AT_12V * FEET_PER_REV / 60 * 0.681818))
#define MTR_MPH_TO_SPEED(mph)      ((mph)   / ((1./3200) * MOTOR_RPM_AT_12V * FEET_PER_REV / 60 * 0.681818))

// motor ctlrs
#define LEFT_MOTOR                   "/dev/ttyACM1"       // USB serial device
#define RIGHT_MOTOR                  "/dev/ttyACM0"       // USB serial device

// encoders
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
