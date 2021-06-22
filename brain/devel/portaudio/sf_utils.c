#include <stdio.h>
#include <sndfile.h>
#include <string.h>
#include <stdlib.h>

#include <sf_utils.h>

// reference:
//   apt install libsndfile1-dev
//   file:///usr/share/doc/libsndfile1-dev/html/api.html
//   https://github.com/libsndfile/libsndfile

int sf_write_wav_file(char *filename, float *data, int max_chan, int max_data, int sample_rate)
{
    SNDFILE *file;
    SF_INFO  sfinfo;
    int      cnt;

    if ((max_data % max_chan) != 0) {
        printf("ERROR: max_data=%d must be a multiple of max_chan=%d\n", max_data, max_chan);
        return -1;
    }

    memset(&sfinfo, 0, sizeof (sfinfo));
    sfinfo.frames     = max_data / max_chan;
    sfinfo.samplerate = sample_rate;
    sfinfo.channels   = max_chan;
    sfinfo.format     = SF_FORMAT_WAV | SF_FORMAT_FLOAT;

    file = sf_open(filename, SFM_WRITE, &sfinfo);
    if (file == NULL) {
        printf("ERROR: sf_open '%s'\n", filename);
        return -1;
    }

    cnt = sf_write_float(file, data, max_data);
    printf("XXX cnt %d\n", cnt);
    if (cnt != max_data) {
        printf("ERROR: sf_write_float, cnt=%d items=%d\n", cnt, max_data);
        sf_close(file);
        return -1;
    }

    sf_close(file);

    return 0;
}

// caller must free returned data when done
int sf_read_wav_file(char *filename, float **data, int *max_chan, int *max_data, int *sample_rate)
{
    SNDFILE *file;
    SF_INFO  sfinfo;
    int      cnt, items;
    float   *d;

    memset(&sfinfo, 0, sizeof (sfinfo));

    file = sf_open(filename, SFM_READ, &sfinfo);
    if (file == NULL) {
        printf("ERROR: sf_open '%s'\n", filename);
        return -1;
    }

    items = sfinfo.frames * sfinfo.channels;
    d = calloc(items, sizeof(float));

    cnt = sf_read_float(file, d, items);
    if (cnt != items) {
        printf("ERROR: sf_read_float, cnt=%d items=%d\n", cnt, items);
        sf_close(file);
    }

    *data        = d;
    *max_chan    = sfinfo.channels;
    *max_data    = sfinfo.frames * sfinfo.channels;
    *sample_rate = sfinfo.samplerate;

    sf_close(file);

    return 0;
}

#ifdef TEST
// gcc -Wall -O2 -o t1 -I. -DTEST sf_utils.c -lsndfile -lm
#include <math.h>
int main(int argc, char **argv) 
{
    #define SECS 5
    #define SAMPLE_RATE 48000
    #define MAX_CHAN 2
    #define MAX_DATA (SECS * SAMPLE_RATE * MAX_CHAN)

    printf("Init Write data ...\n");
    int i, rc;
    float write_data[MAX_DATA];
    for (i = 0; i < MAX_DATA; i += MAX_CHAN) {
        int freq = 500;
        write_data[i] = sin((2*M_PI) * freq * ((double)i/SAMPLE_RATE));
        write_data[i+1] = write_data[i];
    }

    printf("Write file ...\n");
    rc = sf_write_wav_file("test.wav", write_data, MAX_CHAN, MAX_DATA, SAMPLE_RATE);
    if (rc < 0) {
        printf("ERROR: sf_write_wav_file failed\n");
        return 1;
    }

    printf("Read file ...\n");
    float *read_data;
    int max_chan, max_data, sample_rate;
    rc = sf_read_wav_file("test.wav", &read_data, &max_chan, &max_data, &sample_rate);
    if (rc < 0) {
        printf("ERROR: sf_read_wav_file failed\n");
        return 1;
    }
    printf("  max_chan=%d max_data=%d sample_rate=%d\n", max_chan, max_data, sample_rate);

    printf("Readback compare ...\n");
    if (memcmp(write_data, read_data, 100000*sizeof(float)) != 0) {
        printf("ERROR: readback compare failed\n");
        return 1;
    }

    printf("Freeing ...\n");
    free(read_data);

    printf("Terminating ...\n");
    return 0;
}
#endif
