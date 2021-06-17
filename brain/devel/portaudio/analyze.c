#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>

#include <file_utils.h>
#include <misc.h>

//
// defines
//


//
// variables
//

static float *chan_data[32];
static int    max_data;
static int    max_chan;
static int    sample_rate;

static char *file_name;

//
// prototypes
//

static double correlate(float *d1, float *d2, int len);
static double get_max(double *x, int len);
static char *stars(int n);
static void smooth(float *data, int max_data);

// -----------------  MAIN  ----------------------------------

int main(int argc, char **argv)
{
    float *x, *y;
    double c[1000], max;
    int rc, len, i, chana=-1, chanb=-1;

    if (argc != 4) {
        printf("ERROR argc\n");
        return 1;
    }

    file_name = argv[1];
    sscanf(argv[2], "%d", &chana);
    sscanf(argv[3], "%d", &chanb);
    if (chana == -1 || chanb == -1) {
        printf("ERROR invalid args\n");
        return 1;
    }

    rc = file_read(file_name, &max_chan, &max_data, &sample_rate, chan_data);
    if (rc < 0) {
        printf("file_read failed\n");
        return 1;
    }

    // smooth data
    printf("SMOOTHING\n");
    for (i = 0; i < max_chan; i++) {
        smooth(chan_data[i], max_data);
    }

//#define START  31200
//#define END    36000

#if 1
#define START 10000
#define END (max_data-20000)

    x = chan_data[chana] + START;
    len = (END-START);

    printf("\nFILE_NAME=%s  MAX_DATA=%d  CHANS=%d %d  SAMPLES=%d %0.1f secs\n\n", 
         file_name, max_data, chana, chanb, len, (double)len/sample_rate);

#define RANGE 30
    for (i = 0; i <= 2*RANGE; i++) {
        y = chan_data[chanb] + START-RANGE + i;
        c[i] = correlate(x,y,len);
        //printf("%d %f\n", i, c[i]);
    }
#else
#define RANGE 1
    c[0] = 1;
    c[1] = 5;
    c[2] = 2;
#endif

    max = get_max(c, 2*RANGE);
    printf("MAX %f\n", max);

    for (i = 0; i <= 2*RANGE; i++) {
        int num_stars =  nearbyint(c[i] / max * 100);
        printf("%3d %f - %s\n", 
               i-RANGE, 
               c[i], 
               stars(num_stars));
    }

    // starting at ctr, look back until find negative cross corr
    int start = -1;
    for (i = RANGE; i >= 0; i--) {
        if (c[i] < 0) break;
        start = i;
    }

    if (start == -1) {
        printf("no start\n");
        return 1;
    }
    printf("start = %d\n", start);

    interp_point_t p[100];
    double sum = 0, answer;
    int cnt=0;
    for (i = start; c[i] > 0; i++) {
        sum += c[i];
        p[cnt].x = sum;
        p[cnt].y = i-RANGE;
        printf("interp point   %d = %f %f\n", cnt, p[cnt].x, p[cnt].y);
        cnt++;
    }
    printf("SUM  %f\n", sum);
    answer = interpolate(p, cnt, sum/2) + 0.5;
    printf("answer %0.2f\n", answer);

    return 0;
}

static char *stars(int n)
{
    static char array[1000];

    if (n < 0) n = 0;
    memset(array, '*', n);
    array[n] = '\0';
    return array;
}

static double get_max(double *x, int len) 
{
    int i;
    double max = x[0];
    for (i = 1; i < len; i++) {
        if (x[i] > max) max = x[i];
    }
    return max;
}

static double correlate(float *d1, float *d2, int len)
{
    double sum = 0;
    int i;

    for (i = 0; i < len; i++) {
        sum += d1[i] * d2[i];
#if 0
        //sum += 
            //d1[i] * d1[i] * d1[i] *
            //d2[i] * d2[i] * d2[i];
        if (d1[i] > 0 && d2[i] > 0) {
            sum += d1[i] + d2[i];
        } else if (d1[i] < 0 && d2[i] < 0) {
            sum -= d1[i] + d2[i];
        } else {
            sum += d1[i] + d2[i];
        }
#endif
    }

    return sum;
}




static void smooth(float *data, int max_data)
{
    int i;
    for (i = 1; i < max_data; i++) {
        //data[i] = 0.9 * data[i-1] + 0.1 * data[i];
        data[i] = 0.8 * data[i-1] + 0.2 * data[i];
        //data[i] = 0.7 * data[i-1] + 0.7 * data[i];
    }
}

