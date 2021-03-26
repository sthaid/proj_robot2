#ifndef __CURRENT_H__
#define __CURRENT_H__

#ifdef __cplusplus
extern "C" {
#endif

int current_init(void);
int current_read(int adc_chan, double *current);

#ifdef __cplusplus
}
#endif

#endif

