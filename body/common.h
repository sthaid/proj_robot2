#ifndef __COMMON_H__
#define __COMMON_H__

//
// includes
//

// standard linux header files
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <math.h>

// body hardware , and network interface definitios
#include <body.h>
#include <body_network_intfc.h>

// utils
#include <gpio.h>
#include <timer.h>
#include <misc.h>

// devices
#include <mc.h>
#include <encoder.h>
#include <proximity.h>
#include <button.h>
#include <current.h>
#include <oled.h>
#include <env.h>
#include <imu.h>

//
// general defines
//

// dest must be a char array, and not a char *
#define safe_strcpy(dest, src) \
    do { \
        strncpy(dest, src, sizeof(dest)-1); \
        (dest)[sizeof(dest)-1] = '\0'; \
    } while (0)

//
// files
//

// oled_ctlr.c
#define MAX_OLED_STR       5
#define MAX_OLED_STR_SIZE  10
typedef char oled_strs_t[MAX_OLED_STR][MAX_OLED_STR_SIZE];
int oled_ctlr_init(void);
void oled_ctlr_exit(char *str);
oled_strs_t *oled_get_strs(void);

// drive.c
#define MIN_MTR_SPEED 650
#define MAX_MTR_SPEED 2000
int drive_init(void);
void drive_run(struct msg_drive_proc_s *dpm);
int drive_sleep(uint64_t duration_us);
void drive_emer_stop(void);

// drive.c routines called from drive_procs.c
int drive_fwd(double feet, double mph);
int drive_rev(double feet, double mph);
int drive_rotate(double degrees, double rpm);
int drive_stop(void);

// drive_procs.c
int drive_proc(struct msg_drive_proc_s *dpm);

// drive_cal.c
int drive_cal_file_read(void);
int drive_cal_file_write(void);
void drive_cal_tbl_print(void);
int drive_cal_proc(void);
int drive_cal_cvt_mph_to_left_motor_speed(double mph, int *left_mtr_speed);
int drive_cal_cvt_mph_to_right_motor_speed(double mph, int *right_mtr_speed);

#endif
