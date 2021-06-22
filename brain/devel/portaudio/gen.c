#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>

#include <pa_utils.h>

//
// defines
//

#define SAMPLE_RATE         48000  // samples per sec
#define MAX_CHAN            2
#define DEFAULT_DURATION    10
#define DEFAULT_START_FREQ  100
#define DEFAULT_END_FREQ    1000

#define MIN_FREQ            100
#define MAX_FREQ            10000

//
// variables
//

static int    freq_start = DEFAULT_START_FREQ;
static int    freq_end = DEFAULT_END_FREQ;
static int    duration = DEFAULT_DURATION;
static bool   white_noise = false;

static float *data;
static int    max_data;

//
// prototypes
//

static void init_sine_data(void);
static void init_white_noise_data(void);

// -----------------  MAIN  -------------------------------------------------------

int main(int argc, char **argv)
{
    char *output_device = DEFAULT_OUTPUT_DEVICE;

    // usage: gen [-d outdev] [-f freq_start[,freq_end]] [-t duration_secs] [-w] [-h]

    // parse options
    while (true) {
        int ch = getopt(argc, argv, "d:f:t:wh");
        int cnt;
        if (ch == -1) {
            break;
        }
        switch (ch) {
        case 'd':
            output_device = optarg;
            break;
        case 'f':
            cnt = sscanf(optarg, "%d,%d", &freq_start, &freq_end);
            if (cnt == 0) {
                printf("ERROR: invalid frequency range '%s'\n", optarg);
                return 1;
            }

            if (cnt == 1) {
                freq_end = freq_start;
            }

            if (freq_start < MIN_FREQ || freq_start > MAX_FREQ ||
                freq_end < MIN_FREQ || freq_end > MAX_FREQ ||
                freq_end < freq_start)
            {
                printf("ERROR: invalid frequency range %d - %d\n", freq_start, freq_end);
                return 1;
            }
            break;
        case 't':
            cnt = sscanf(optarg, "%d", &duration);
            if (cnt != 1 || duration < 1 || duration > 60) {
                printf("ERROR: invalid duration '%s'\n", optarg);
                return 1;
            }
            break;
        case 'w':
            white_noise = true;
            break;
        case 'h':
            printf("usage: gen [-d outdev] [-f freq_start[,freq_end]] [-t duration_secs] [-w] [-h]\n");
            return 0;
            break;
        default:
            return 1;
        };
    }

    // initialize sound data buffer 
    if (white_noise == false) {
        init_sine_data();
    } else {
        init_white_noise_data();
    }

    // init pa_utils
    if (pa_init() < 0) {
        printf("ERROR: pa_init failed\n");
        return 1;
    }

    // print params
    printf("\nduation = %d secs   freq = %d - %d   sample_rate = %d\n\n",
           duration, freq_start, freq_end, SAMPLE_RATE);

    // play sound data
    if (pa_play(output_device, MAX_CHAN, max_data, SAMPLE_RATE, data) < 0) {
        printf("ERROR: pa_play failed\n");
        return 1;
    }

    // done
    return 0;
}

static void init_sine_data(void)
{
    double freq;
    int i;

    max_data = duration * SAMPLE_RATE * MAX_CHAN;
    data = malloc(max_data * sizeof(float));

    for (i = 0; i < max_data; i += MAX_CHAN) {
        freq = freq_start + (freq_end - freq_start) * ((double)i / (max_data - MAX_CHAN));
        data[i] = sin((2*M_PI) * freq * ((double)i/SAMPLE_RATE));
        data[i+1] = data[i];
    }

    if (nearbyint(freq) != freq_end) {
        printf("ERROR: BUG freq %0.3f not equal freq_end %d\n", freq, freq_end);
        exit(1);
    }
}

static void init_white_noise_data(void)
{
    int i;

    max_data = duration * SAMPLE_RATE;
    data = malloc(max_data * sizeof(float));
    
    for (i = 0; i < max_data; i++) {
        data[i] = ((double)random() / RAND_MAX) - 0.5;
    }
};
