#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <dlfcn.h>
#include <pthread.h>

#include <pv_porcupine.h>
#include <pa_utils.h>

//
// defines
//

#define SAMPLE_RATE 16000

#define LIB_PATH    "../../repos/Porcupine/lib/linux/x86_64/libpv_porcupine.so"
#define MODEL_PATH  "../../repos/Porcupine/lib/common/porcupine_params.pv"

#define MAX_KEYWORD_PATHS (sizeof(keyword_paths) / sizeof(keyword_paths[0]))

#define MAX_PLAYBACK_DATA (3*SAMPLE_RATE)  // 3 seconds

//
// variables
//

static const char *(*pv_status_to_string_func)(pv_status_t);
static int (*pv_sample_rate_func)(void);
static pv_status_t (*pv_porcupine_init_func)(const char *, int, const char *const *, const float *, pv_porcupine_t **);
static void (*pv_porcupine_delete_func)(pv_porcupine_t *);
static pv_status_t (*pv_porcupine_process_func)(pv_porcupine_t *, const int16_t *, int *);
static int (*pv_porcupine_frame_length_func)(void);

static const char *keyword_paths[] = {
    "../../repos/Porcupine/resources/keyword_files/linux/porcupine_linux.ppn",
    "../../repos/Porcupine/resources/keyword_files/linux/bumblebee_linux.ppn",
    "../../repos/Porcupine/resources/keyword_files/linux/grasshopper_linux.ppn",
            };
static float sensitivities[MAX_KEYWORD_PATHS];

static pv_porcupine_t *porcupine;

static short *porcupine_sound_data;
static int    porcupine_frame_length;

static float  playback_data[MAX_PLAYBACK_DATA];
static int    playback_cnt = -1;

//
// prototypes
//

static void *playback_thread(void *cx);
static int recv_mic_data(const float *frame, void *cx);
static void init_lib_syms(void);

// -----------------  MAIN  ------------------------------------------------------

int main(int argc, char **argv)
{
    pv_status_t     pvrc;
    int             rc;
    int             sample_rate;
    pthread_t       tid;

    // use line buffered stdout
    setlinebuf(stdout);

    // open the porcupine library and get function addresses
    init_lib_syms();

    // init sensitivities array, using same value for each of the keywords being monitored
    for (int i = 0; i < MAX_KEYWORD_PATHS; i++) {
        sensitivities[i] = 0.8;
    }

    // initialize the porcupine wake word detection engine, to monitor for
    // the keyword(s) contained in keyword_paths
    pvrc = pv_porcupine_init_func(MODEL_PATH, MAX_KEYWORD_PATHS, keyword_paths, sensitivities, &porcupine);
    if (pvrc != PV_STATUS_SUCCESS) {
        printf("ERROR: pv_porcupine_init, %s\n", pv_status_to_string_func(pvrc));
        return 1;
    }

    // verify the sample rate required by porcupine matches the SAMPLE_RATE value
    // that this program s built with
    sample_rate = pv_sample_rate_func();
    if (sample_rate != SAMPLE_RATE) {
        printf("ERROR: sample_rate=%d is not expected %d\n", sample_rate, SAMPLE_RATE);
        return 1;
    }

    // get the porcupine frame length, this is the number of sound data values
    // that the pv_porcupine_process routine expects to receive for each call
    porcupine_frame_length = pv_porcupine_frame_length_func();
    printf("porcupine_frame_length = %d\n", porcupine_frame_length);
    porcupine_sound_data = calloc(porcupine_frame_length, sizeof(short));

    // initialize portaudio utils; these utils provide a simplified interface 
    // to the portaudio library
    rc = pa_init();
    if (rc < 0) {
        printf("ERROR: pa_init\n");
        return 1;
    }

    // create playback thread;
    // this thread is used to play short sound segments from after keyword detection
    pthread_create(&tid, NULL, playback_thread, NULL);

    // call portaudio util to start acquiring mic data;
    // the recv_mic_data callback is called repeatedly with the mic data;
    // the pa_record2 blocks until recv_mic_data returns non-zero (blocks forever in this pgm)
    rc =  pa_record2(DEFAULT_INPUT_DEVICE,  // xxx  use topp define
                     1,              // max_chan
                     SAMPLE_RATE,    // sample_rate
                     recv_mic_data,  // callback
                     NULL,           // cx passed to recv_mic_data
                     0);             // discard_samples count
    if (rc < 0) {
        printf("ERROR: pa_record2\n");
        return 1;
    }

    // done
    return 0;
}

// -----------------  PLAYBCK THREAD  --------------------------------

static void *playback_thread(void *cx)
{
    int rc;

    while (true) {
        // if the playback buffer is full then
        //   call pa_play to play the playback_data
        //   reset playback_cnt to -1, indicating that we're done with this playback_data
        // endif
        if (playback_cnt == MAX_PLAYBACK_DATA) {
            rc = pa_play(DEFAULT_OUTPUT_DEVICE, 1, MAX_PLAYBACK_DATA, SAMPLE_RATE, playback_data);
            if (rc < 0) {
                printf("ERROR: pa_play failed\n");
                exit(1);
            }

            playback_cnt = -1;
        }

        // short sleep, 10 ms
        usleep(10000);
    }

    return NULL;
}

// -----------------  RECV MIC DATA  ---------------------------------

static int recv_mic_data(const float *frame, void *cx)
{
    int         keyword;
    pv_status_t pvrc;

    static int  psd_cnt;

    // if a keyword has been detected then
    //   Save 3 secs of playback_data.
    //   This buffer will be played by the playback_thread once it is full.
    // else
    //   Save sound data in porcupine_sound_data array.
    //   Once this buffer is full it is passed to pv_porcupine_process_func 
    //    to check for wake word detection.
    //   If a wake word is detected then playback_cnt is set to 0, so that
    //    subsequent calls to recv_mic_data wiill save the playback_data.
    // endif

    if (playback_cnt >= 0) {
        if (playback_cnt < MAX_PLAYBACK_DATA) {
            playback_data[playback_cnt++] = frame[0];
        }
    } else {
        porcupine_sound_data[psd_cnt++] = frame[0] * 32767;

        if (psd_cnt == porcupine_frame_length) {
            psd_cnt = 0;

            keyword = -1;
            pvrc = pv_porcupine_process_func(porcupine, porcupine_sound_data, &keyword);
            if (pvrc != PV_STATUS_SUCCESS) {
                printf("ERROR: pv_porcupine_process, %s\n", pv_status_to_string_func(pvrc));
                exit(1);
            }

            if (keyword != -1) {
                printf("detected keyword %d\n", keyword);
                playback_cnt = 0;
            }
        }
    }

    return 0;
}

// -----------------  OBTAIN ACCESS TO PORCUPINE LIBRARY  ------------

static void init_lib_syms(void)
{
    void *lib;

    #define GET_LIB_SYM(symname) \
        do { \
            symname##_func = dlsym(lib, #symname); \
            if (symname##_func == NULL) { \
                printf("ERROR: dlsym %s, %s\n", #symname, dlerror()); \
                exit(1); \
            } \
        } while (0)

    lib = dlopen(LIB_PATH, RTLD_NOW);
    if (lib == NULL) {
        printf("ERROR: filed to open %s, %s\n", LIB_PATH, strerror(errno));
        exit(1);
    }

    GET_LIB_SYM(pv_status_to_string);
    GET_LIB_SYM(pv_sample_rate);
    GET_LIB_SYM(pv_porcupine_init);
    GET_LIB_SYM(pv_porcupine_delete);
    GET_LIB_SYM(pv_porcupine_process);
    GET_LIB_SYM(pv_porcupine_frame_length);
}
