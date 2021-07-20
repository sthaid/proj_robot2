#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <libgen.h>
#include <errno.h>
#include <dlfcn.h>
#include <pthread.h>
#include <fcntl.h>  
#include <inttypes.h>  
#include <sys/types.h>
#include <sys/wait.h>

#include <pv_porcupine.h>
#include <pa_utils.h>

//
// defines
//

#ifdef RASPBERRY_PI
    #define LIB_PATH      "../repos/Porcupine/lib/raspberry-pi/cortex-a72/libpv_porcupine.so"
    #define MODEL_PATH    "../repos/Porcupine/lib/common/porcupine_params.pv"
    #define INPUT_DEVICE  "seeed-4mic-voicecard"
    #define OUTPUT_DEVICE "USB"
#else
    #define LIB_PATH      "../repos/Porcupine/lib/linux/x86_64/libpv_porcupine.so"
    #define MODEL_PATH    "../repos/Porcupine/lib/common/porcupine_params.pv"
    #define INPUT_DEVICE  DEFAULT_INPUT_DEVICE
    #define OUTPUT_DEVICE DEFAULT_OUTPUT_DEVICE
#endif

#define PORC_AND_LC_SAMPLE_RATE 16000
#define RECORD_SAMPLE_RATE      48000

#define FRAME_LENGTH  512

#define MAX_SOUND_DATA (10 * PORC_AND_LC_SAMPLE_RATE / FRAME_LENGTH * FRAME_LENGTH)

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
#ifdef RASPBERRY_PI
    "../repos/Porcupine/resources/keyword_files/raspberry-pi/porcupine_raspberry-pi.ppn",
    "../repos/Porcupine/resources/keyword_files/raspberry-pi/bumblebee_raspberry-pi.ppn",
    "../repos/Porcupine/resources/keyword_files/raspberry-pi/grasshopper_raspberry-pi.ppn",
#else
    "../repos/Porcupine/resources/keyword_files/linux/porcupine_linux.ppn",
    "../repos/Porcupine/resources/keyword_files/linux/bumblebee_linux.ppn",
    "../repos/Porcupine/resources/keyword_files/linux/grasshopper_linux.ppn",
#endif
            };
static float sensitivities[] = {
    0.8,
    0.8,
    0.8,
            };

static pv_porcupine_t *porcupine;

static short    sound_data[MAX_SOUND_DATA];
static uint64_t idx_sound_data;
static uint64_t livecaption_thread_start_idx;
static int      detected_keyword = -1;

//
// prototypes
//

static int recv_mic_data(const void *frame, void *cx);
static void *livecaption_thread(void *cx);
static void process_transcript(char *transcript);
static void init_lib_syms(void);
static void run_program(pid_t *prog_pid, int *fd_to_prog, int *fd_from_prog, char *prog, ...);
static char *keyword_name(int idx);
static uint64_t microsec_timer(void);

// -----------------  MAIN  ------------------------------------------------------

int main(int argc, char **argv)
{
    pv_status_t     pvrc;
    int             rc;
    int             sample_rate;
    int             frame_length;
    pthread_t       tid;

    #define MAX_KEYWORD_PATHS (sizeof(keyword_paths) / sizeof(keyword_paths[0]))

    // use line buffered stdout
    setlinebuf(stdout);

    // open the porcupine library and get function addresses
    init_lib_syms();

    // initialize the porcupine wake word detection engine, to monitor for
    // the keyword(s) contained in keyword_paths
    pvrc = pv_porcupine_init_func(MODEL_PATH, MAX_KEYWORD_PATHS, keyword_paths, sensitivities, &porcupine);
    if (pvrc != PV_STATUS_SUCCESS) {
        printf("ERROR: pv_porcupine_init, %s\n", pv_status_to_string_func(pvrc));
        return 1;
    }

    // verify sample_rate and frame_length exepected by porcupine agree
    // with defines used by this program
    sample_rate = pv_sample_rate_func();
    if (sample_rate != PORC_AND_LC_SAMPLE_RATE) {
        printf("ERROR: sample_rate=%d is not expected %d\n", sample_rate, PORC_AND_LC_SAMPLE_RATE);
        return 1;
    }
    frame_length = pv_porcupine_frame_length_func();
    if (frame_length != FRAME_LENGTH) {
        printf("ERROR: frame_length=%d is not expected %d\n", frame_length, FRAME_LENGTH);
        return 1;
    }

    // initialize portaudio utils; 
    // these utils provide a simplified interface to the portaudio library
    rc = pa_init();
    if (rc < 0) {
        printf("ERROR: pa_init\n");
        return 1;
    }

    // create the livecaption_thread;
    pthread_create(&tid, NULL, livecaption_thread, NULL);

    // call portaudio util to start acquiring mic data;
    // the recv_mic_data callback is called repeatedly with the mic data;
    // the pa_record2 blocks until recv_mic_data returns non-zero (blocks forever in this pgm)
    rc =  pa_record2(INPUT_DEVICE,
                     1,                   // max_chan
                     RECORD_SAMPLE_RATE,  // sample_rate
                     PA_INT16,            // 16 bit signed data
                     recv_mic_data,       // callback
                     NULL,                // cx passed to recv_mic_data
                     0);                  // discard_samples count
    if (rc < 0) {
        printf("ERROR: pa_record2\n");
        return 1;
    }

    // done
    return 0;
}

// -----------------  RECV MIC DATA  ---------------------------------

static int recv_mic_data(const void *frame_arg, void *cx)
{
    const short *frame = frame_arg;

    // note - this is called at sample rate 48000 Hz

    // direction of arrival (DOA) analysis 
    // TBD

    // discard 2 out of 3 frames, so the sample rate for the code below is 16000
    static int call_cnt;
    if (++call_cnt < 3) {
        return 0;
    }
    call_cnt = 0;

    // save single channel sound_data, which is used by 
    // - call to pv_porcupine_process_func
    // - the livecaption thread
    sound_data[idx_sound_data % MAX_SOUND_DATA] = frame[0];
    idx_sound_data++;

    // if livecaption thread is active then return
    if (livecaption_thread_start_idx > 0) {
        return 0;
    }

    // if we have the length of sound_data needed by pv_porcupine_process then 
    //   call pv_porcupine_process
    //   if keyword is detected then 
    //     wake the livecaption thread
    //   endif
    // endif
    if ((idx_sound_data % FRAME_LENGTH) == 0) {
        int keyword;
        short *sd;
        pv_status_t pvrc;

        sd = &sound_data[(idx_sound_data-FRAME_LENGTH) % MAX_SOUND_DATA];
        pvrc = pv_porcupine_process_func(porcupine, sd, &keyword);
        if (pvrc != PV_STATUS_SUCCESS) {
            printf("ERROR: pv_porcupine_process, %s\n", pv_status_to_string_func(pvrc));
            exit(1);
        }

        if (keyword != -1) {
            detected_keyword = keyword;
            __sync_synchronize();
            livecaption_thread_start_idx = idx_sound_data;
        }
    }

    return 0;
}

// -----------------  LIVECAPTION THREAD  ----------------------------

static void *livecaption_thread(void *cx)
{
    int      fd_to_lc, fd_from_lc, rc, flags;
    uint64_t sd_idx;
    pid_t    lc_pid;
    char     transcript[2000];
    uint64_t start_time;

    while (true) {
        // wait for indication to start livecaption
        while (livecaption_thread_start_idx == 0) {
            usleep(30000);
        }

        // print the detected_keyword
        printf("DETECTED KEYWORD: %s\n", keyword_name(detected_keyword));
        detected_keyword = -1;

        // execute livecaption
        run_program(&lc_pid, &fd_to_lc, &fd_from_lc, "./livecaption", NULL);

        // set fd used to read livecaption stdout to non blocking
        flags = fcntl(fd_from_lc, F_GETFL, 0); 
        flags |= O_NONBLOCK; 
        fcntl(fd_from_lc, F_SETFL, flags); 

        // int variables
        // XXX check earlier
        if ((livecaption_thread_start_idx % FRAME_LENGTH) != 0) {
            printf("ERROR: livecaption_thread_start_idx=0x%" PRIx64 " is not a multiple of FRAME_LENGTH=%d\n",
                   livecaption_thread_start_idx, FRAME_LENGTH);
            exit(1);
        }
        sd_idx = livecaption_thread_start_idx;
        memset(transcript, 0, sizeof(transcript));
        start_time = microsec_timer();

        while (true) {
            // if FRAME_LENGTH sound_data values are available then
            //   write block of sound_data to livecaption
            // endif
            if (idx_sound_data >= sd_idx + (FRAME_LENGTH + 1)) {
                short *sd = &sound_data[sd_idx % MAX_SOUND_DATA];

                rc = write(fd_to_lc, sd, FRAME_LENGTH*sizeof(short));
                if ((rc != FRAME_LENGTH*sizeof(short)) && (rc != -1 || errno != EPIPE)) {
                    printf("ERROR: failed write to livecaption, rc=%d, %s\n", rc, strerror(errno));
                    exit(1);
                }
                sd_idx += FRAME_LENGTH;
            }

            // perform non blocking read of livecaption stdout
            rc = read(fd_from_lc, transcript, sizeof(transcript)-1);
            if (rc < 0 && errno != EWOULDBLOCK) {
                printf("ERROR: failed read from livecaption, %s\n", strerror(errno));
                exit(1);
            }

            // if we have a result from livecaption then print it and break
            if (rc > 0) {
                break;
            }

            // timeout if there is not a result from livecaption in 5 secs
            if (microsec_timer() - start_time > 5000000) {
                printf("TIMEDOUT WAITING FOR TRANSCRIPT\n");
                break;
            }

            // short sleep, 10ms
            usleep(10000);
        }

        // close fds and call waitpid
        close(fd_to_lc);
        close(fd_from_lc);
        waitpid(lc_pid, NULL, 0);

        // if transcript is available then process it
        if (transcript[0] != '\0') {
            process_transcript(transcript);
        }

        // setting livecaption_thread_start_idx to 0 restarts the recv_mic_data
        // callback routine monitoring for wake word
        livecaption_thread_start_idx = 0;
    }

    return NULL;
}

// -----------------  PROCESS TRANSCRIPT  -----------------------------

static void process_transcript(char *transcript)
{
    pid_t st_pid;
    int fd_to_st, fd_from_st;
    short data[1000000];
    int len, fd, rc;

    // remove newline char at the end of transcript
    transcript[strcspn(transcript, "\n")] = 0;

    // run synthesize_text to convert transcript text to raw audio file
    // xxx this could be a routine
    printf("RUN_PROG synthesize_text, '%s'\n", transcript);
    run_program(&st_pid, &fd_to_st, &fd_from_st, "./synthesize_text", "--text", transcript, NULL);
    close(fd_to_st);
    close(fd_from_st);
    waitpid(st_pid, NULL, 0);
    printf("RUN_PROG done\n");

    // read the raw audio file, output.raw
    // xxx this could be a routine
    fd = open("output.raw", O_RDONLY);
    if (fd < 0) {
        printf("ERROR: open output.raw, %s\n", strerror(errno));
        return;
    }
    len = read(fd, data, sizeof(data));
    if (len <= 0) {
        printf("ERROR: read, len=%d, %s\n", len, strerror(errno));
        close(fd);
        return;
    }
    close(fd);

    // play the transcript
    printf("PA_PLAY len %d\n", len);
    rc = pa_play(OUTPUT_DEVICE, 1, len/2-1000, 24000, PA_INT16, data+1000);
    printf("PA_PLAY done, rc=%d\n", rc);
}

// -----------------  INIT ACCESS TO PORCUPINE LIBRARY  ---------------
// XXX reorg below

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

// -----------------  RUN PROGRAM USING FORK AND EXEC  ---------------

static void run_program(pid_t *prog_pid, int *fd_to_prog, int *fd_from_prog, char *prog, ...)
{
    char *args[100];
    int argc=0;
    int pipe_to_child[2], pipe_from_child[2];
    pid_t pid;
    va_list ap;
    sigset_t set;

    // block SIGPIPE
    sigemptyset(&set);
    sigaddset(&set,SIGPIPE);
    sigprocmask(SIG_BLOCK, &set, NULL);

    // construct args array
    args[argc++] = prog;
    va_start(ap, prog);
    while (true) {
        args[argc] = va_arg(ap, char*);
        if (args[argc] == NULL) break;
        argc++;
    }
    va_end(ap);

    // create pipes for prog input and output
    // - pipefd[0] is read end, pipefd[1] is write end
    pipe(pipe_to_child);
    pipe(pipe_from_child);

    // fork
    pid = fork();
    if (pid == -1) {
        printf("ERROR: fork failed, %s\n", strerror(errno));
        exit(1);
    }

    // if pid == 0, child is running, else parent
    if (pid == 0) {
        // child ..
        // close unused ends of the pipes
        close(pipe_to_child[1]);
        close(pipe_from_child[0]);

        // attach the 2 pipes to stdin and stdout for the child
        dup2(pipe_to_child[0], 0);
        dup2(pipe_from_child[1], 1);

        // execute the program
        execvp(prog, args);
        printf("ERROR: execvp %s, %s\n", prog, strerror(errno));
        exit(1);        
    } else {
        // parent ... 
        // close unused ends of the pipes
        close(pipe_to_child[0]);
        close(pipe_from_child[1]);

        // return values to caller
        *fd_to_prog = pipe_to_child[1];;
        *fd_from_prog = pipe_from_child[0];
        *prog_pid = pid;
    }
}

// -----------------  MISC  ------------------------------------------

static char *keyword_name(int idx)
{
    static char str[200];
    char *name, *p;

    if (idx == -1) {
        return "????";
    }

    strcpy(str, keyword_paths[idx]);
    name = basename(str);
    p = strchr(name, '_');
    if (p) *p = '\0';

    return name;
}

static uint64_t microsec_timer(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC,&ts);
    return  ((uint64_t)ts.tv_sec * 1000000) + ((uint64_t)ts.tv_nsec / 1000);
}

