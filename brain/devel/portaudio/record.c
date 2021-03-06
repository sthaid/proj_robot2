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

#define DEFAULT_IN_DEV       SEEED_4MIC_VOICECARD
#define DEFAULT_DURATION     5      // secs
#define DEFAULT_FILE_NAME    "record.wav"

//
// variables
//

static int   duration     = DEFAULT_DURATION;
static char *input_device = DEFAULT_IN_DEV;
static char *file_name    = DEFAULT_FILE_NAME;

// -----------------  MAIN  -------------------------------------------------------

int main(int argc, char **argv)
{
    #define USAGE \
    "usage: record [-d indev] [-f file_name.wav] [-T duration_secs]"

    // parse options
    while (true) {
        int ch = getopt(argc, argv, "d:f:T:h");
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

    // init max_chan, max_data, and data;
    // use 4 channels for seeed-4mic-voicecard, otherwise 1
    int max_chan = (strcmp(input_device, SEEED_4MIC_VOICECARD) == 0 ? 4 : 1);
    int max_data = duration * SAMPLE_RATE * max_chan;
    float *data = calloc(max_data, sizeof(float));

    // init pa_utils
    if (pa_init() < 0) {
        printf("ERROR: pa_init failed\n");
        return 1;
    }

    // record sound data
    // XXX comment 48000
    if (pa_record(input_device, max_chan, max_data, SAMPLE_RATE, PA_FLOAT32, data, 48000) < 0) {
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
