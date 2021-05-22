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
int drive_init(void);
void drive_run(struct msg_drive_proc_s *dpm);
void drive_emer_stop(void);
bool drive_emer_stop_occurred(void);

// drive.c routines called from drive_procs.c
int drive_straight_cal(double cal_feet);
int drive_mag_cal(double num_rot);
int drive_fwd(double feet, double mph, bool stop_motors_flag);
int drive_rev(double feet, double mph);
int drive_rotate(double degrees, double fudge);
int drive_rotate_to_heading(double heading, double fudge);
int drive_radius(double desired_degrees, double radius_feet, bool stop_motors_flag, double fudge);

// drive_procs.c
int drive_proc(struct msg_drive_proc_s *dpm);

#endif
