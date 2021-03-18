#ifndef __PROXIMITY_H__
#define __PROXIMITY_H__

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_PROXIMITY 1

int proximity_init(void);
bool proximity_check(int id, int *sum, int *poll_rate);

#ifdef __cplusplus
}
#endif

#endif
