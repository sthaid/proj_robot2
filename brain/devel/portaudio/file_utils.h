#ifndef __FILE_UTILS_H__
#define __FILE_UTILS_H__

int file_write(char *fn, int max_chan, int max_data, int sample_rate, float **chan_data);
int file_read(char *fn, int *max_chan, int *max_data, int *sample_rate, float **chan_data);
void file_read_free(float **chan_data);

#endif
