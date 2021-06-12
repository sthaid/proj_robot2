#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>

//
// defines
//

#define MAX_CHANNEL  4
#define SAMPLE_RATE  48000  // samples per sec
#define DURATION     1      // secs
#define MAX_DATA     (DURATION * SAMPLE_RATE)

#define GET_MIC_DATA_FILENAME "get_mic_data.dat"

//
// variables
//

static float  data[MAX_CHANNEL][MAX_DATA];
static bool   data_ready;

//
// prototypes
//

static double correlate(float *d1, float *d2, int len);
static double get_max(double *x, int len);
static char *stars(int n);
static int get_mic_data_read_file(void);

// -----------------  MAIN  ----------------------------------

int main(int argc, char **argv)
{
    float *x, *y;
    double c[1000], max;
    int rc, len, i, chana=-1, chanb=-1;

    setlinebuf(stdout);

    rc = get_mic_data_read_file();
    if (rc < 0) {
	return 1;
    }

    if (argc != 3) {
	printf("ERROR argc\n");
	return 1;
    }

    sscanf(argv[1], "%d", &chana);
    sscanf(argv[2], "%d", &chanb);
    if (chana == -1 || chanb == -1) {
	printf("ERROR invalid args\n");
	return 1;
    }
    printf("CHANS %d %d\n", chana, chanb);

    x = data[chana] + 2000;
    len = MAX_DATA - 3000;

    for (i = 0; i <= 40; i++) {
	y = data[chanb] + 1980 + i;
	c[i] = correlate(x,y,len);
	printf("%d %f\n", i, c[i]);
    }

    max = get_max(c, 40);
    printf("MAX %f\n", max);

    for (i = 0; i <= 40; i++) {
	int num_stars =  nearbyint(c[i] / max * 100);
	printf("%3d %f - %s\n", 
               i-20, 
               c[i], 
               stars(num_stars));
    }

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
    }

    return sum;
}

// -----------------  UTILS  ---------------------------------

static int get_mic_data_read_file(void)
{
    int fd, len;

    printf("reading %s\n", GET_MIC_DATA_FILENAME);

    fd = open(GET_MIC_DATA_FILENAME, O_RDONLY);
    if (fd < 0) {
        printf("open %s, %s\n", GET_MIC_DATA_FILENAME, strerror(errno));
        return -1;
    }

    len = read(fd, data, sizeof(data));
    if (len != sizeof(data)) {
        printf("read %s len=%d, %s\n", GET_MIC_DATA_FILENAME, len, strerror(errno));
        return -1;
    }

    close(fd); 
    data_ready = true;
    return 0;
}

