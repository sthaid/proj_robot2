#include <utils.h>

#include <sndfile.h>

// reference:
//   apt install libsndfile1-dev
//   file:///usr/share/doc/libsndfile1-dev/html/api.html
//   https://github.com/libsndfile/libsndfile

void sf_init(void)
{
    // nothing needed
}

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
    INFO("XXX cnt %d\n", cnt);
    if (cnt != max_data) {
        ERROR("sf_write_short, cnt=%d items=%d\n", cnt, max_data);
        sf_close(file);
        return -1;
    }

    sf_close(file);

    return 0;
}

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
    d = calloc(items, sizeof(short));

    // read the wav file data
    cnt = sf_read_short(file, d, items);
    if (cnt != items) {
        ERROR("sf_read_short, cnt=%d items=%d\n", cnt, items);
        sf_close(file);
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

// caller must supply a sufficiently large data buffer
int sf_read_wav_file2(char *filename, short *data, int *max_chan, int *max_data, int *sample_rate)
{
    SNDFILE *file;
    SF_INFO  sfinfo;
    int      cnt, items;

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

    // read the wav file data
    items = sfinfo.frames * sfinfo.channels;
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
