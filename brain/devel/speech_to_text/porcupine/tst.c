#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dlfcn.h>

#include <pv_porcupine.h>
#include <pa_utils.h>

//
// defines
//

#define SAMPLE_RATE 16000

#define LIB_PATH    "../../repos/Porcupine/lib/linux/x86_64/libpv_porcupine.so"
#define MODEL_PATH  "../../repos/Porcupine/lib/common/porcupine_params.pv"

#define MAX_KEYWORD_PATHS (sizeof(keyword_paths) / sizeof(keyword_paths[0]))

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
    "../../repos/Porcupine/resources/keyword_files/linux/bumblebee_linux.ppn"
            };
static float sensitivities[MAX_KEYWORD_PATHS];

static short *sound_data;

static int frame_length;
    pv_porcupine_t *porcupine;

//
// prototypes
//

static void init_lib_syms(void);
static int put_frame(const float *frame, void *cx);

// -------------------------------------------------------------------------------

int main(int argc, char **argv)
{
    pv_status_t     pvrc;
    int             rc;
    int             sample_rate;

    setlinebuf(stdout);

    init_lib_syms();

    for (int i = 0; i < MAX_KEYWORD_PATHS; i++) {
        sensitivities[i] = 0.5;
    }

    pvrc = pv_porcupine_init_func(MODEL_PATH, MAX_KEYWORD_PATHS, keyword_paths, sensitivities, &porcupine);
    if (pvrc != PV_STATUS_SUCCESS) {
        printf("ERROR: pv_porcupine_init, %s\n", pv_status_to_string_func(pvrc));
        return 1;
    }

    // verify sample rate
    sample_rate = pv_sample_rate_func();
    if (sample_rate != SAMPLE_RATE) {
        printf("ERROR: sample_rate=%d is not expected %d\n", sample_rate, SAMPLE_RATE);
        return 1;
    }

    // get frame_length
    // XXX bytes ?
    frame_length = pv_porcupine_frame_length_func();
    printf("frame_length = %d\n", frame_length);
    sound_data = malloc(frame_length*2);  // xxx use calloc
    memset(sound_data, 0, frame_length*2);

    rc = pa_init();
    if (rc < 0) {
        printf("ERROR: pa_init\n");
        return 1;
    }

    rc =  pa_record2(DEFAULT_INPUT_DEVICE,
                     1,            // max_chan
                     SAMPLE_RATE,  // sample_rate  XXX 
                     put_frame,    // callback
                     NULL,         // put_frame cx
                     0);           // discard_samples count
    if (rc < 0) {
        printf("ERROR: pa_record2\n");
        return 1;
    }

    return 0;
}

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

static int put_frame(const float *frame, void *cx)
{
    pv_status_t pvrc;
    int keyword;

    static int cnt;

    sound_data[cnt++] = frame[0] * 32767;

    if (cnt == frame_length) {
        keyword = -1;

        pvrc = pv_porcupine_process_func(porcupine, sound_data, &keyword);
        if (pvrc != PV_STATUS_SUCCESS) {
            printf("ERROR: pv_porcupine_process, %s\n", pv_status_to_string_func(pvrc));
            exit(1);
        }

        cnt = 0;

        if (keyword != -1) {
            printf("detected keyword %d\n", keyword);
        }
    }

    return 0;
}

#if 0
    #define MAX 160
    static int cnt;
    static short buffer[MAX];

    // collect 160 values, in 16 bit format, and write them to stdout
    buffer[cnt++] = frame[0] * 32767;
    if (cnt == MAX) {
        if (write(1, buffer, sizeof(buffer)) < 0) {
            printf("ERROR: write failed\n");
            return -1;
        }
        cnt = 0;
    }

    // continue
    return 0;
#endif


#if 0
/*
    Copyright 2018-2020 Picovoice Inc.

    You may not use this file except in compliance with the license. A copy of the license is located in the "LICENSE"
    file accompanying this source.

    Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on
    an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the
    specific language governing permissions and limitations under the License.
*/

#include <alsa/asoundlib.h>
#include <dlfcn.h>
#include <signal.h>
#include <stdio.h>

#include "pv_porcupine.h"

static volatile bool is_interrupted = false;

void interrupt_handler(int _) {
    (void) _;
    is_interrupted = true;
}

int main(int argc, char *argv[]) {
    if (argc != 6) {
        fprintf(stderr, "usage : %s library_path model_path keyword_path sensitivity input_audio_device\n", argv[0]);
        exit(1);
    }

    signal(SIGINT, interrupt_handler);

    const char *library_path = argv[1];
    const char *model_path = argv[2];
    const char *keyword_path = argv[3];
    const float sensitivity = (float) atof(argv[4]);
    const char *input_audio_device = argv[5];

    void *porcupine_library = dlopen(library_path, RTLD_NOW);
    if (!porcupine_library) {
        fprintf(stderr, "failed to open library.\n");
        exit(1);
    }

    char *error = NULL;

    pv_porcupine_t *porcupine = NULL;
    pv_status_t status = pv_porcupine_init_func(model_path, 1, &keyword_path, &sensitivity, &porcupine);
    if (status != PV_STATUS_SUCCESS) {
        fprintf(stderr, "'pv_porcupine_init' failed with '%s'\n", pv_status_to_string_func(status));
        exit(1);
    }




    const int32_t frame_length = pv_porcupine_frame_length_func();
    printf("XXXX frame_len  %d\n", frame_length);

    int16_t *pcm = malloc(frame_length * sizeof(int16_t));
    if (!pcm) {
        fprintf(stderr, "failed to allocate memory for audio buffer\n");
        exit(1);
    }

    while (!is_interrupted) {
        const int count = snd_pcm_readi(alsa_handle, pcm, frame_length);
        if (count < 0) {
            fprintf(stderr, "'snd_pcm_readi' failed with '%s'\n", snd_strerror(count));
            exit(1);
        } else if (count != frame_length) {
            fprintf(stderr, "read %d frames instead of %d\n", count, frame_length);
            exit(1);
        }

        int32_t keyword_index = -1;
        status = pv_porcupine_process_func(porcupine, pcm, &keyword_index);
        if (status != PV_STATUS_SUCCESS) {
            fprintf(stderr, "'pv_porcupine_process' failed with '%s'\n", pv_status_to_string_func(status));
            exit(1);
        }
        if (keyword_index != -1) {
            fprintf(stdout, "detected keyword\n");
        }
    }

    free(pcm);
    snd_pcm_close(alsa_handle);
    pv_porcupine_delete_func(porcupine);
    dlclose(porcupine_library);

    return 0;
}
#endif
