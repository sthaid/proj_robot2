#ifndef __BUTTON_H__
#define __BUTTON_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>

int button_init(int max_info, ...);
int button_pressed(int id);

#ifdef __cplusplus
}
#endif

#endif
