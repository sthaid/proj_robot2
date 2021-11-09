#include <utils.h>

#include <sndfile.h>

// reference:
//   apt install libsndfile1-dev
//   file:///usr/share/doc/libsndfile1-dev/html/api.html
//   https://github.com/libsndfile/libsndfile

// -----------------  INIT  ----------------------------------------

void sf_init(void)
{
    // nothing needed
}

// -----------------  WRITE WAV FILE  ------------------------------

int sf_write_wav_file(char *filename, short *data, int max_chan, int max_data, int sample_rate)
{
    SNDFILE *file;
    SF_INFO  sfinfo;
    int      cnt;

    // max_data must be a multiple of max_chan
    if ((max_data % max_chan) != 0) {
        ERROR("max_data=%d must be a multiple of max_chan=%d\n", max_data, max_chan);
        return -1;
    }

    // init SF_INFO struct
    memset(&sfinfo, 0, sizeof (sfinfo));
    sfinfo.frames     = max_data / max_chan;
    sfinfo.samplerate = sample_rate;
    sfinfo.channels   = max_chan;
    sfinfo.format     = SF_FORMAT_WAV | SF_FORMAT_PCM_16;

    // open filename for writing
    file = sf_open(filename, SFM_WRITE, &sfinfo);
    if (file == NULL) {
        ERROR("sf_open '%s'\n", filename);
        return -1;
    }

    // write the file
    cnt = sf_write_short(file, data, max_data);
    if (cnt != max_data) {
        ERROR("sf_write_short, cnt=%d items=%d\n", cnt, max_data);
        sf_close(file);
        return -1;
    }

    sf_close(file);

    return 0;
}

// -----------------  READ WAV FILE  -------------------------------

// caller must free returned data when done
int sf_read_wav_file(char *filename, short **data, int *max_chan, int *max_data, int *sample_rate)
{
    SNDFILE *file;
    SF_INFO  sfinfo;
    int      cnt, items;
    short   *d;

    // preset return values
    *data = NULL;
    *max_chan = 0;
    *max_data = 0;
    *sample_rate = 0;

    // open wav file and get info
    memset(&sfinfo, 0, sizeof (sfinfo));
    file = sf_open(filename, SFM_READ, &sfinfo);
    if (file == NULL) {
        ERROR("sf_open '%s'\n", filename);
        return -1;
    }

    // allocate memory for the data
    items = sfinfo.frames * sfinfo.channels;
    d = malloc(items*sizeof(short));

    // read the wav file data
    cnt = sf_read_short(file, d, items);
    if (cnt != items) {
        ERROR("sf_read_short, cnt=%d items=%d\n", cnt, items);
        sf_close(file);
        return -1;
    }

    // close file
    sf_close(file);

    // return values
    *data        = d;
    *max_chan    = sfinfo.channels;
    *max_data    = items;
    *sample_rate = sfinfo.samplerate;
    return 0;
}

// on input max_data is the total number of shorts in caller's data buffer
int sf_read_wav_file2(char *filename, short *data, int *max_chan, int *max_data, int *sample_rate)
{
    SNDFILE *file;
    SF_INFO  sfinfo;
    int      cnt, items;
    int      max_data_orig = *max_data;

    // preset return values
    *max_chan = 0;
    *max_data = 0;
    *sample_rate = 0;

    // open wav file and get info
    memset(&sfinfo, 0, sizeof (sfinfo));
    file = sf_open(filename, SFM_READ, &sfinfo);
    if (file == NULL) {
        ERROR("sf_open '%s'\n", filename);
        return -1;
    }

    // limit number of items being read to not overflow caller's buffer
    items = sfinfo.frames * sfinfo.channels;
    if (items > max_data_orig) {
        items = max_data_orig;
    }

    // read the wav file data
    cnt = sf_read_short(file, data, items);
    if (cnt != items) {
        ERROR("sf_read_short, cnt=%d items=%d\n", cnt, items);
        sf_close(file);
    }

    // close file
    sf_close(file);

    // return values
    *max_chan    = sfinfo.channels;
    *max_data    = items;
    *sample_rate = sfinfo.samplerate;
    return 0;
}

// -----------------  GEN FREQ SWEEP WAV FILE  ---------------------

int sf_gen_wav_file(char *filename, int freq_start, int freq_end, int duration, int max_chan, int sample_rate)
{
    int i, j, max_data, idx=0, rc;
    double k, t, freq, val, last_val, offset;
    short *data;

    INFO("gen_wav_file: %s freq_range=%d-%d  duration=%d  max_chan=%d  sample_rate=%d\n",
         filename, freq_start, freq_end, duration, max_chan, sample_rate);

    // allocate data
    max_data = duration * sample_rate * max_chan;
    data = malloc(max_data * sizeof(short));

    // init data
    k = log((double)freq_end/freq_start) / duration;
    offset = 0;
    last_val = 0;
    freq = freq_start;
    for (i = 0; i < duration*sample_rate; i++) {
        t = (double)i / sample_rate;
        val = sin((2*M_PI) * freq * t + offset);
        if (val >= 0 && last_val < 0) {
            // calc new frequency and offset when sine wave goes from negative to positive
            freq = freq_start * exp(k * t);
            offset = asin(val) - (2*M_PI * freq * t);
        }
        last_val = val;

        for (j = 0; j < max_chan; j++) {
            data[idx++] = nearbyint(val * 32767);
        }
    }
    assert(idx == max_data);

    // write file
    rc = sf_write_wav_file(filename, data, max_chan, max_data, sample_rate);
    if (rc < 0) {
        ERROR("failed to create wav file %s\n", filename);
        return -1;
    }
    INFO("created %s\n", filename);

    // free data, and return success
    free(data);
    return 0;
}
