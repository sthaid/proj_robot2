#ifndef __CURRENT_H__
#define __CURRENT_H__

#ifdef __cplusplus
extern "C" {
#endif

// Notes:
// - current_init varargs: int adc_chan, ...

int current_init(int max_info, ...);    // returns -1 on error, else 0

double current_get(int id);

#ifdef __cplusplus
}
#endif

#endif

