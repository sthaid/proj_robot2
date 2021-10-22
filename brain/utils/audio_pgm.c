#define _GNU_SOURCE  // xxx should this be in common, is thee another way?
                     // xxx or always use _GN_SOURCCE

#include <utils.h>

// variables
static audio_shm_t *shm;
static bool end_program;

// prototypes
static void sig_hndlr(int sig);
static void audio_out_init(void);
static int recv_mic_data(const void *frame, void *cx);
static void set_affinity_and_realtime(void);

// -----------------  MAIN  ---------------------------------------------------

int main(int argc, char **argv)
{
    int rc, fd;

    // register for SIGINT and SIGTERM
    static struct sigaction act;
    act.sa_handler = sig_hndlr;
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGTERM, &act, NULL);

    // init logging
    log_init(NULL, false, false);
    INFO("AUDIO INITIALIZING\n");

    // open and map AUDIO_SHM
    fd = shm_open(AUDIO_SHM, O_RDWR, 0666);
    if (fd < 0) {
        FATAL("audio shm_open %s, %s\n", AUDIO_SHM, strerror(errno));
    }
    shm = mmap(NULL, sizeof(audio_shm_t), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (shm == NULL) {
        FATAL("audio mmap %s, %s\n", AUDIO_SHM, strerror(errno));
    }
    
    // init utils used by this program
    misc_init();
    pa_init();

    // initialize audio output capability
    audio_out_init();

    // call pa_record2 to start receiving the 4 channel respeaker mic data;
    // the recv_mic_data callback routine is called with mic data frames
    INFO("AUDIO RUNNING\n");
    rc =  pa_record2("seeed-4mic-voicecard",
                     4,                   // max_chan
                     48000,               // sample_rate
                     PA_INT16,            // 16 bit signed data
                     recv_mic_data,       // callback
                     NULL,                // cx passed to recv_mic_data
                     0);                  // discard_samples count
    if (rc < 0) {
        ERROR("error pa_record2\n");
    }

    // terminate
    INFO("AUDIO TERMINATING\n");
    return 0;
}

static void sig_hndlr(int sig)
{
    end_program = true;
}

// -----------------  AUDIO OUTPUT  -------------------------------------------

#define BEEP_DURATION_MS 200
#define BEEP_FREQUENCY   800
#define BEEP_AMPLITUDE   6000
#define MAX_BEEP_DATA    (48000 * BEEP_DURATION_MS / 1000)

static short beep_data[MAX_BEEP_DATA];

static void *audio_out_thread(void *cx);
static int audio_out_get_frame(void *data_arg, void *cx);

// - - - - - - - - - - - 

static void audio_out_init(void)
{
    int i;
    pthread_t tid;

    for (i = MAX_BEEP_DATA/4; i < MAX_BEEP_DATA*3/4; i++) {
        beep_data[i] = BEEP_AMPLITUDE * sin(i * (2*M_PI / MAX_BEEP_DATA * BEEP_FREQUENCY));
    }

    pthread_create(&tid, NULL, audio_out_thread, NULL);
}

static void *audio_out_thread(void *cx)
{
    while (true) {
        // wait
        while (shm->execute == false) {
            usleep(10000); // xxx or shorten
        }

        // play
        pa_play2("USB", 2, 48000, PA_INT16, audio_out_get_frame, NULL);

        // done
        shm->execute = false;
    }

    return NULL;
}

// xxx could run this at sample_rate=24000 by setting PA_ALSA_PLUGHW=1 env var
static int audio_out_get_frame(void *data_arg, void *cx)
{
    short *data = data_arg;

    assert(shm->beep_count >= 0);
    assert(shm->max_data >= 0);

    if (shm->beep_count > 0) {
        static int idx;

        data[0] = beep_data[idx];
        data[1] = beep_data[idx];
        idx++;

        if (idx >= MAX_BEEP_DATA) {
            idx = 0;
            shm->beep_count--;
        }
        return 0;
    }

    if (shm->max_data > 0) {
        static int idx;

        data[0] = shm->data[idx/2];
        data[1] = shm->data[idx/2];
        idx++;

        if (idx >= shm->max_data*2) {
            idx = 0;
            shm->max_data = 0;
        }
        return 0;
    }

    return -1;
}

// -----------------  AUDIO INPUT FROM RESPEAKER 4 CHAN MIC  ------------------

static int recv_mic_data(const void *frame, void *cx)
{
    static int cnt;
    static bool first_call = true;

    // on first call set affinity and realtime
    if (first_call) {
        set_affinity_and_realtime();
        first_call = false;
    }

    // check if this program is terminating; if so,
    // return -1 to stop receiving mic data, and pa_record2 will return
    if (end_program) {
        return -1;
    }

    // store frame in array, to be processed by the proc_mic_data_thread, in brain.c
    memcpy(shm->frames[shm->fidx+cnt], frame, sizeof(shm->frames[0]));
    cnt++;

    // publish every 48 values
    if (cnt == 48) {
        __sync_synchronize();
        shm->fidx = (shm->fidx + 48) % 48000;
        cnt = 0;
    }

    // continue receiving mic data
    return 0;
}

// xxx creator can do this, once the tid is known
static void set_affinity_and_realtime(void)
{
    struct sched_param param;
    cpu_set_t cpu_set;
    int rc;

    INFO("audio setting realtime and affinity\n");

    // set affinity to cpu 3
    CPU_ZERO(&cpu_set);
    CPU_SET(3, &cpu_set);
    rc = sched_setaffinity(0,sizeof(cpu_set_t),&cpu_set);
    if (rc < 0) {
        FATAL("audio sched_setaffinity, %s\n", strerror(errno));
    }

    // set realtime priority
    memset(&param, 0, sizeof(param));
    param.sched_priority = 95;
    rc = sched_setscheduler(0, SCHED_FIFO, &param);
    if (rc < 0) {
        FATAL("audio sched_setscheduler, %s\n", strerror(errno));
    }
}
