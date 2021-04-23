#ifndef __COMMON_H__
#define __COMMON_H__

// standard linux
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <curses.h>
#include <misc.h>

// body hardware defs
#include <body.h>

// devices
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

// oled_ctlr ...
#define MAX_OLED_STR       5
#define MAX_OLED_STR_SIZE  10
typedef char oled_strs_t[MAX_OLED_STR][MAX_OLED_STR_SIZE];
int oled_ctlr_init(void);
oled_strs_t *oled_get_strs(void);

#endif
