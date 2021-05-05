#ifndef __COMMON_H__
#define __COMMON_H__

// dest must be a char array, and not a char *
#define safe_strcpy(dest, src) \
    do { \
        strncpy(dest, src, sizeof(dest)-1); \
        (dest)[sizeof(dest)-1] = '\0'; \
    } while (0)

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

// oled_ctlr.c
#define MAX_OLED_STR       5
#define MAX_OLED_STR_SIZE  10
typedef char oled_strs_t[MAX_OLED_STR][MAX_OLED_STR_SIZE];
int oled_ctlr_init(void);
void oled_ctlr_exit(char *str);
oled_strs_t *oled_get_strs(void);

// drive.c
int drive_init(void);
void drive_run(int proc_id);
int drive_sleep(uint64_t duration_us);

// drive.c routines called from drive_procs.c
int drive_fwd(double mph, double feet);
int drive_rew(double mph, double feet);
int drive_xxx(double lmph, double rmph, double feet);
int drive_rotate(double mph, double feet);  // angle xxx
int drive_stop(void);

// drive_cal.c
int drive_cal_file_read(void);
int drive_cal_file_write(void);
void drive_cal_tbl_print(void);
int drive_cal_proc(void);
int drive_cal_cvt_mph_to_left_motor_speed(double mph, int *left_mtr_speed);
int drive_cal_cvt_mph_to_right_motor_speed(double mph, int *right_mtr_speed);

// drive_procs.c
extern int (*drive_procs_tbl[])(void);

#endif
