#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>

#include <portaudio.h>
#include <pa_utils.h>
#include <sf_utils.h>
#include <file_utils.h>

//
// defines
//

//#define DEBUG

#define SEEED_4MIC_VOICECARD "seeed-4mic-voicecard"

#define SAMPLE_RATE  48000  // samples per sec
#define DURATION     5      // secs
#define MAX_DATA     (DURATION * SAMPLE_RATE)

//
// variables
//

// -----------------  MAIN  -------------------------------------------------------

int main(int argc, char **argv)
{
    char *input_device = SEEED_4MIC_VOICECARD;
    char *file_name = "record.dat";
    int   max_chan, rc;

    // usage: record [-d indev] [-f file_name]

    // parse options
    while (true) {
        int ch = getopt(argc, argv, "d:f:h");
        if (ch == -1) {
            break;
        }
        switch (ch) {
        case 'd':
            input_device = optarg;
            break;
	case 'f':
	    file_name = optarg;
	    break;
        case 'h':
            printf("usage: record [-d indev] [-f file_name]\n");
            return 0;
            break;
        default:
            return 1;
        };
    }

    // use 4 channels for seeed-4mic-voicecard, otherwise 1
    max_chan = (strcmp(input_device, SEEED_4MIC_VOICECARD) == 0 ? 4 : 1);

    // init pa_utils
    if (pa_init() < 0) {
        printf("ERROR: pa_init failed\n");
        return 1;
    }

    // record sound data
    float *chan_data[max_chan];
    for (int chan = 0; chan < max_chan; chan++) {
	chan_data[chan] = calloc(MAX_DATA, sizeof(float));
    }
    if (pa_record(input_device, max_chan, MAX_DATA, SAMPLE_RATE, chan_data) < 0) {
        printf("ERROR: pa_play failed\n");
        return 1;
    }

    // write sound data to file
    printf("writing recorded data to %s\n", file_name);
    rc = file_write(file_name, max_chan, MAX_DATA, SAMPLE_RATE, chan_data);
    if (rc < 0) {
	printf("ERROR: file_write failed\n");
	return 1;
    }

#ifdef DEBUG
    // print average data for each channel
    printf("\n");
    for (int chan = 0; chan < max_chan; chan++) {
        float sum = 0;
        for (int i = 0; i < MAX_DATA; i++) {
            sum += fabsf(chan_data[chan][i]);
        }
        printf("avg data for chan %d = %0.3f\n", chan, sum/MAX_DATA);
    }

    // write wav file for each channel
    for (int chan = 0; chan < max_chan; chan++) {
        char filename[100];
        sprintf(filename, "record_%d.wav", chan);
        sf_create_wav(chan_data[chan], MAX_DATA, SAMPLE_RATE, filename);
    }
#endif

    // done
    return 0;
}
