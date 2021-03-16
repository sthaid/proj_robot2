#ifndef __RELAY_H__
#define __RELAY_H__

#include <stdarg.h>

int relay_init(int num, ...);
void relay_ctrl(int gpionum, bool enable);

#endif
