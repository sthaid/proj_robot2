#include <stdio.h>
#include <unistd.h>

#include <pa_utils.h>

static int put_frame(const float *frame, void *cx);

int main(int argc, char **argv)
{
    int rc;

    rc = pa_init();
    if (rc < 0) {
        printf("ERROR: pa_init\n");
        return 1;
    }

    rc =  pa_record2(DEFAULT_INPUT_DEVICE, 
                     1,         // max_chan
                     16000,     // sample_rate
                     put_frame, // callback
                     NULL,      // put_frame cx
                     0);        // discard_samples count
    if (rc < 0) {
        printf("ERROR: pa_record2\n");
        return 1;
    }
}

static int put_frame(const float *frame, void *cx)
{
    #define MAX 160
    static int cnt;
    static short buffer[MAX];

    // collect 160 values, in 16 bit format, and write them to stdout
    buffer[cnt++] = frame[0] * 32767;
    if (cnt == MAX) {
        if (write(1, buffer, sizeof(buffer)) < 0) {
            printf("ERROR: write failed\n");
            return -1;
        }
        cnt = 0;
    }

    // continue
    return 0;
}

