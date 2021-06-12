#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include <file_utils.h>

#define FILE_MAGIC 0x123

typedef struct {
    int magic;
    int max_chan;
    int max_data;
    int sample_rate;
    int reserved[12];
} file_hdr_t;

int file_write(char *fn, int max_chan, int max_data, int sample_rate, float **chan_data) 
{
    file_hdr_t hdr;
    int fd, i;

    // create the file
    fd = open(fn, O_CREAT|O_TRUNC|O_WRONLY, 0644);
    if (fd < 0) {
        printf("ERROR: open %s, %s\n", fn, strerror(errno));
        return -1;
    }

    // write the hdr
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic       = FILE_MAGIC;
    hdr.max_chan    = max_chan;
    hdr.max_data    = max_data;
    hdr.sample_rate = sample_rate;
    if (write(fd, &hdr, sizeof(hdr)) != sizeof(hdr)) {
        printf("ERROR write %s, %s\n", fn, strerror(errno));
        close(fd);
        return -1;
    }

    // write the channel data
    for (i = 0; i < max_chan; i++) {
        float *d = chan_data[i];
        if (write(fd, d, max_data*sizeof(float)) != max_data*sizeof(float)) {
            printf("ERROR write %s, %s\n", fn, strerror(errno));
            close(fd);
            return -1;
        }
    }

    // close, and return success
    close(fd);
    return 0;
}

int file_read(char *fn, int *max_chan, int *max_data, int *sample_rate, float **chan_data)
{
    int fd, data_len, i;
    float *data;
    file_hdr_t hdr;

    // open file
    fd = open(fn, O_RDONLY);
    if (fd < 0) {
        printf("ERROR: open %s, %s\n", fn, strerror(errno));
        return -1;
    }

    // read the hdr, and check magic
    if (read(fd, &hdr, sizeof(hdr)) != sizeof(hdr)) {
        printf("ERROR read %s, %s\n", fn, strerror(errno));
        close(fd);
        return -1;
    }
    if (hdr.magic != FILE_MAGIC) {
        printf("ERROR: bad magic 0x%x\n", hdr.magic);
        close(fd);
        return -1;
    }

    // allocate memory for the data
    data_len = hdr.max_chan * hdr.max_data * sizeof(float);
    data = malloc(data_len);
    if (data == NULL) {
        printf("ERROR: malloc\n");
        close(fd);
        return -1;
    }

    // read the data
    if (read(fd, data, data_len) != data_len) {
        printf("ERROR read %s, %s\n", fn, strerror(errno));
        close(fd);
        return -1;
    }

    // close file
    close(fd);

    // fill in return values
    *max_chan    = hdr.max_chan;
    *max_data    = hdr.max_data;
    *sample_rate = hdr.sample_rate;
    for (i = 0; i < hdr.max_chan; i++) {
        chan_data[i] = data + (i * hdr.max_data);
    }

    // success
    return 0;
}

void file_read_free(float **chan_data)
{
    free(chan_data[0]);
}

