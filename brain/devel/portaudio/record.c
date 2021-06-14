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
#define SAMPLE_RATE          48000  // samples per sec
#define DEFAULT_DURATION     12     // secs

//
// variables
//

static int duration = DEFAULT_DURATION;

// -----------------  MAIN  -------------------------------------------------------

int main(int argc, char **argv)
{
    char *input_device = SEEED_4MIC_VOICECARD;
    char *file_name = "record.dat";
    int   max_chan, rc;

    // usage: record [-d indev] [-f file_name] [-t duration_secs]

    // parse options
    while (true) {
        int ch = getopt(argc, argv, "d:f:t:h");
        int cnt;
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
        case 't':
            cnt = sscanf(optarg, "%d", &duration);
            if (cnt != 1 || duration < 1 || duration > 60) {
                printf("ERROR: invalid duration '%s'\n", optarg);
                return 1;
            }
            break;
        case 'h':
            printf("usage: record [-d indev] [-f file_name] [-t duration_secs]\n");
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
    int max_data = duration * SAMPLE_RATE;

    for (int chan = 0; chan < max_chan; chan++) {
        chan_data[chan] = calloc(max_data, sizeof(float));
    }
    if (pa_record(input_device, max_chan, max_data, SAMPLE_RATE, chan_data) < 0) {
        printf("ERROR: pa_play failed\n");
        return 1;
    }

    // write sound data to file
    printf("writing recorded data to %s\n", file_name);
    rc = file_write(file_name, max_chan, max_data, SAMPLE_RATE, chan_data);
    if (rc < 0) {
        printf("ERROR: file_write failed\n");
        return 1;
    }

#ifdef DEBUG
    // print average data for each channel
    printf("\n");
    for (int chan = 0; chan < max_chan; chan++) {
        float sum = 0;
        for (int i = 0; i < max_data; i++) {
            sum += fabsf(chan_data[chan][i]);
        }
        printf("avg data for chan %d = %0.3f\n", chan, sum/max_data);
    }

    // write wav file for each channel
    for (int chan = 0; chan < max_chan; chan++) {
        char filename[100];
        sprintf(filename, "record_%d.wav", chan);
        sf_create_wav(chan_data[chan], max_data, SAMPLE_RATE, filename);
    }
#endif

    // done
    return 0;
}
