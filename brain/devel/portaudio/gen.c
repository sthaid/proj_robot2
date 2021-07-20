#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>

#include <pa_utils.h>
#include <sf_utils.h>

//
// defines
//

#define SAMPLE_RATE         48000  // samples per sec
#define MAX_CHAN            2
#define DEFAULT_DURATION    5
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
static char  *file_name;

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

    #define USAGE \
    "usage: gen [-d outdev] [-t type] [-f file_name.wav] [-T secs] [-h]\n" \
    "       - type: freq | freq_start,freq_end | white\n" \
    "       - outdev can be set to 'none'"

    // parse options
    while (true) {
        int ch = getopt(argc, argv, "d:t:f:T:h");
        int cnt;
        if (ch == -1) {
            break;
        }
        switch (ch) {
        case 'd':
            output_device = optarg;
            break;
        case 't':
            if (strcmp(optarg, "white") == 0) {
                white_noise = true;
                break;
            }

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
        case 'f':
            file_name = optarg;
            if (strstr(file_name, ".wav") == NULL) {
                printf("ERROR: file_name must have '.wav' extension\n");
                return 1;
            }
            break;
        case 'T':
            cnt = sscanf(optarg, "%d", &duration);
            if (cnt != 1 || duration < 1 || duration > 60) {
                printf("ERROR: invalid duration '%s'\n", optarg);
                return 1;
            }
            break;
        case 'h':
            printf("%s\n", USAGE);
            return 0;
            break;
        default:
            return 1;
        };
    }

    // print params
    printf("duation=%d secs  sample_rate=%d", duration, SAMPLE_RATE);
    if (!white_noise) {
        printf("  freq=%d,%d", freq_start, freq_end);
    } else {
        printf("  white_noise");
    }
    if (file_name) {
        printf("  file_name=%s", file_name);
    }
    printf("\n");

    // initialize sound data buffer 
    if (white_noise == false) {
        init_sine_data();
    } else {
        init_white_noise_data();
    }

    // if file_name provided then create wav file
    if (file_name) {
        printf("writing file %s\n", file_name);
        sf_write_wav_file(file_name, data, MAX_CHAN, max_data, SAMPLE_RATE);
    }

    // if output_device is not "none" then play the sound data
    if (strcmp(output_device, "none") != 0) {
        // init pa_utils
        if (pa_init() < 0) {
            printf("ERROR: pa_init failed\n");
            return 1;
        }

        // play sound data
        if (pa_play(output_device, MAX_CHAN, max_data, SAMPLE_RATE, PA_FLOAT32, data) < 0) {
            printf("ERROR: pa_play failed\n");
            return 1;
        }
    }

    // done
    return 0;
}

static void init_sine_data(void)
{
    double freq = 500;
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

    max_data = duration * SAMPLE_RATE * MAX_CHAN;
    data = malloc(max_data * sizeof(float));
    
    for (i = 0; i < max_data; i++) {
        data[i] = 2 * (((double)random() / RAND_MAX) - 0.5);
    }
}
