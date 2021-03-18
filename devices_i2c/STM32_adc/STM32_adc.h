#ifndef __STM32_ADC_H__
#define __STM32_ADC_H__

#ifdef __cplusplus
extern "C" {
#endif

int STM32_adc_init(int dev_addr);
int STM32_adc_read(int chan, double *voltage);

#ifdef __cplusplus
}
#endif

#endif
