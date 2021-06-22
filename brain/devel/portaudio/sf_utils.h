#ifndef __SF_UTILS_H__
#define __SF_UTILS_H__

int sf_write_wav_file(char *filename, float *data, int max_chan, int max_data, int sample_rate);
int sf_read_wav_file(char *filename, float **data, int *max_chan, int *max_data, int *sample_rate);

#endif
