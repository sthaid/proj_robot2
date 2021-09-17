#include <common.h>

#include <dlfcn.h>
#include <pv_porcupine.h>

//
// defines
//

#define FRAME_LENGTH  512
#define SAMPLE_RATE   16000

#define LIB_PATH      "./devel/repos/Porcupine/lib/raspberry-pi/cortex-a72/libpv_porcupine.so"
#define MODEL_PATH    "./devel/repos/Porcupine/lib/common/porcupine_params.pv"
#define INPUT_DEVICE  "seeed-4mic-voicecard"

// xxx check this list
static const char *keyword_paths[] = {
    "./devel/repos/Porcupine/resources/keyword_files/raspberry-pi/porcupine_raspberry-pi.ppn",
    "./devel/repos/Porcupine/resources/keyword_files/raspberry-pi/bumblebee_raspberry-pi.ppn",
    "./devel/repos/Porcupine/resources/keyword_files/raspberry-pi/grasshopper_raspberry-pi.ppn",
            };
static const float sensitivities[] = {
    0.8,
    0.8,
    0.8,
            };

#define MAX_KEYWORD_PATHS (sizeof(keyword_paths) / sizeof(keyword_paths[0]))

//
// variables
//

static pv_porcupine_t *porcupine;

static const char *(*pv_status_to_string_func)(pv_status_t);
static int (*pv_sample_rate_func)(void);
static pv_status_t (*pv_porcupine_init_func)(const char *, int, const char *const *, const float *, pv_porcupine_t **);
static void (*pv_porcupine_delete_func)(pv_porcupine_t *);
static pv_status_t (*pv_porcupine_process_func)(pv_porcupine_t *, const int16_t *, int *);
static int (*pv_porcupine_frame_length_func)(void);

//
// prototypes
//

static void porcupine_lib_init(void);

// -----------------  WAKE WORD DETECTOR INITIALIZE  --------------------

void wwd_init(void)
{
    pv_status_t pvrc;
    int         sample_rate;
    int         frame_length;

    // open the porcupine library and get function addresses
    porcupine_lib_init();

    // initialize the porcupine wake word detection engine, to monitor for
    // the keyword(s) contained in keyword_paths
    pvrc = pv_porcupine_init_func(MODEL_PATH, MAX_KEYWORD_PATHS, keyword_paths, sensitivities, &porcupine);
    if (pvrc != PV_STATUS_SUCCESS) {
        FATAL("pv_porcupine_init, %s\n", pv_status_to_string_func(pvrc));
    }

    // verify sample_rate and frame_length exepected by porcupine agree
    // with defines used by this program
    sample_rate = pv_sample_rate_func();
    if (sample_rate != SAMPLE_RATE) {
        FATAL("sample_rate=%d is not expected %d\n", sample_rate, SAMPLE_RATE);
    }
    frame_length = pv_porcupine_frame_length_func();
    if (frame_length != FRAME_LENGTH) {
        FATAL("frame_length=%d is not expected %d\n", frame_length, FRAME_LENGTH);
    }
}

static void porcupine_lib_init(void)
{
    void *lib;

    #define GET_LIB_SYM(symname) \
        do { \
            symname##_func = dlsym(lib, #symname); \
            if (symname##_func == NULL) { \
                FATAL("dlsym %s, %s\n", #symname, dlerror()); \
            } \
        } while (0)

    lib = dlopen(LIB_PATH, RTLD_NOW);
    if (lib == NULL) {
        FATAL("filed to open %s, %s\n", LIB_PATH, strerror(errno));
    }

    GET_LIB_SYM(pv_status_to_string);
    GET_LIB_SYM(pv_sample_rate);
    GET_LIB_SYM(pv_porcupine_init);
    GET_LIB_SYM(pv_porcupine_delete);
    GET_LIB_SYM(pv_porcupine_process);
    GET_LIB_SYM(pv_porcupine_frame_length);
}

// -----------------  FEED DATA TO WWD  ---------------------------------

// returns -1:      no wake word detected
//         keyword: which wake word was detected
int wwd_feed(short sound_val)
{
    pv_status_t  pvrc;
    int          keyword;

    static short sd[FRAME_LENGTH];
    static int   max_sd;

    // xxx name sd and max_sd
    sd[max_sd++] = sound_val;
    if (max_sd < FRAME_LENGTH) {
        return -1;
    }

    max_sd = 0;

    //INFO("feed %d\n", sound_val);
    pvrc = pv_porcupine_process_func(porcupine, sd, &keyword);
    if (pvrc != PV_STATUS_SUCCESS) {
        FATAL("pv_porcupine_process, %s\n", pv_status_to_string_func(pvrc));
    }
    if (keyword != -1) INFO("XXX GOT KEYWORD %d\n", keyword);

    return keyword;
}
