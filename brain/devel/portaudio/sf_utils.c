#include <stdio.h>
#include <sndfile.h>
#include <string.h>

#include "sf_utils.h"

// reference:
//   apt install libsndfile1-dev
//   file:///usr/share/doc/libsndfile1-dev/html/api.html

int sf_create_wav(float *data, int max_data, int sample_rate, char *filename)
{
    SNDFILE *file;
    SF_INFO  sfinfo;
    int      cnt;

    memset(&sfinfo, 0, sizeof (sfinfo));

    sfinfo.samplerate = sample_rate;
    sfinfo.frames     = max_data;
    sfinfo.channels   = 1;
    sfinfo.format     = SF_FORMAT_WAV | SF_FORMAT_FLOAT;

    file = sf_open(filename, SFM_WRITE, &sfinfo);
    if (file == NULL) {
        printf("ERROR: sf_open '%s'\n", filename);
        return -1;
    }

    cnt = sf_write_float(file, data, max_data);
    if (cnt != max_data) {
        printf("ERROR: sf_write_float\n");
        sf_close(file);
    }

    sf_close(file);

    return 0;
}
