#ifndef __CURRENT_H__
#define __CURRENT_H__

#ifdef __cplusplus
extern "C" {
#endif

int current_init(int max_info, ...);
int current_read_unsmoothed(int id, double *current);
int current_read(int id, double *current);

#ifdef __cplusplus
}
#endif

#endif

