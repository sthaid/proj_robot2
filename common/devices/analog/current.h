#ifndef __CURRENT_H__
#define __CURRENT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>

int current_init(int max_info, ...);
int current_read(int id, double *current);

#ifdef __cplusplus
}
#endif

#endif

