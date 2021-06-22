#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>

#include <portaudio.h>
#include <pa_utils.h>
#include <sf_utils.h>

//
// defines
//

#define SEEED_4MIC_VOICECARD "seeed-4mic-voicecard"
#define SAMPLE_RATE          48000  // samples per sec
#define DEFAULT_DURATION     10     // secs

//
// variables
//

static int duration = DEFAULT_DURATION;

// -----------------  MAIN  -------------------------------------------------------

int main(int argc, char **argv)
{
    char *input_device = SEEED_4MIC_VOICECARD;
    char *file_name = "record.wav";

    // usage: record [-d indev] [-f file_name.wav] [-t duration_secs]

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
            if (strstr(file_name, ".wav") == NULL) {
                printf("ERROR: file_name must have '.wav' extension\n");
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
        case 'h':
            printf("usage: record [-d indev] [-f file_name.wav] [-t duration_secs]\n");
            return 0;
            break;
        default:
            return 1;
        };
    }

    // use 4 channels for seeed-4mic-voicecard, otherwise 1
    int max_chan = (strcmp(input_device, SEEED_4MIC_VOICECARD) == 0 ? 4 : 1);

    // init pa_utils
    if (pa_init() < 0) {
        printf("ERROR: pa_init failed\n");
        return 1;
    }

    // record sound data
    int max_data = duration * SAMPLE_RATE * max_chan;
    float *data = calloc(max_data, sizeof(float));
    if (pa_record(input_device, max_chan, max_data, SAMPLE_RATE, data) < 0) {
        printf("ERROR: pa_record failed\n");
        return 1;
    }

    // write sound data to wav file
    if (sf_write_wav_file(file_name, data, max_chan, max_data, SAMPLE_RATE) < 0) {
        printf("ERROR sf_write_wav_file failed\n");
        return 1;
    }

    // done
    return 0;
}
