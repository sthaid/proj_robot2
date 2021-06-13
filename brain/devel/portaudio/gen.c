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
#define DURATION            5      // secs
#define MAX_CHAN            2
#define MAX_DATA            (DURATION * SAMPLE_RATE)

#define DEFAULT_FREQ_START  300
#define MIN_FREQ            100
#define MAX_FREQ            10000

//
// variables
//

static int    freq_start = DEFAULT_FREQ_START;
static int    freq_end;
static float  data[MAX_DATA];

//
// prototypes
//

static void init_data(void);

// -----------------  MAIN  -------------------------------------------------------

int main(int argc, char **argv)
{
    char *output_device = DEFAULT_OUTPUT_DEVICE;

    // usage: gen [-d outdev] [freq_start] [freq_end]

    // parse options
    while (true) {
	int ch = getopt(argc, argv, "d:h");
	if (ch == -1) {
	    break;
	}
	switch (ch) {
	case 'd':
	    output_device = optarg;
	    break;
	case 'h':
	    printf("usage: gen [-d outdev] [freq_start] [freq_end]\n");
	    return 0;
	    break;
	default:
	    return 1;
	};
    }

    // determine freq_start and freq_end
    if (argc-optind >= 1 && sscanf(argv[optind], "%d", &freq_start) != 1) {
        printf("ERROR: not a number '%s'\n", argv[optind]);
        return 1;
    }
    freq_end = 2 * freq_start;
    if (argc-optind >= 2 && sscanf(argv[optind+1], "%d", &freq_end) != 1) {
        printf("ERROR: not a number '%s'\n", argv[optind+1]);
        return 1;
    }
        
    // validate frequency range
    if (freq_start < MIN_FREQ || freq_start > MAX_FREQ ||
        freq_end < MIN_FREQ || freq_end > MAX_FREQ ||
        freq_end < freq_start)
    {
        printf("ERROR: invalid frequency range: %d - %d\n", freq_start, freq_end);
        return 1;
    }

    // initialize sound data buffer 
    init_data();

    // init pa_utils
    if (pa_init() < 0) {
	printf("ERROR: pa_init failed\n");
	return 1;
    }

    // play sound data
    float *chan_data[MAX_CHAN] = {NULL, data};
    if (pa_play(output_device, MAX_CHAN, MAX_DATA, SAMPLE_RATE, chan_data) < 0) {
	printf("ERROR: pa_play failed\n");
	return 1;
    }

    // done
    return 0;
}

static void init_data(void)
{
    double freq = freq_start;
    int max_data = 0;

    while (true) {
        data[max_data] = sin((2*M_PI) * freq * ((double)max_data/SAMPLE_RATE));
        max_data++;

        freq += (double)(freq_end - freq_start) / (DURATION * SAMPLE_RATE);

        if (max_data == MAX_DATA) {
            if (nearbyint(freq) != freq_end) {
                printf("ERROR: BUG freq %0.3f not equal freq_end %d\n", freq, freq_end);
                exit(1);
            }
            break;
        }
    }
}

