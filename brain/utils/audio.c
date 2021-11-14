#include <utils.h>

// defines
#define MUTEX_LOCK do { pthread_mutex_lock(&mutex); } while (0)
#define MUTEX_UNLOCK do { pthread_mutex_unlock(&mutex); } while (0)

#define BEEP_SAMPLE_RATE 24000
#define BEEP_DURATION_MS 200
#define BEEP_FREQUENCY   800
#define BEEP_AMPLITUDE   6000
#define MAX_BEEP_DATA    (BEEP_SAMPLE_RATE * BEEP_DURATION_MS / 1000)

// variables
static pthread_t proc_mic_data_tid;
static audio_shm_t *shm;
static bool audio_exitting;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static short beep_data[MAX_BEEP_DATA];

// prototypes
static void audio_exit(void);
static void *proc_mic_data_thread(void *cx);

// -----------------  INIT  -------------------------------------------------

void audio_init(int (*proc_mic_data)(short *frame), int volume)
{
    int rc, fd;

    // if audio pgm is already running, then error
    rc = system("pgrep -x audio");
    if (rc == 0) {
        FATAL("audio pgm is already running\n");
    }

    // set initial volume
    audio_out_set_volume(volume);

    // create and map audio_shm; 
    // open with O_TRUNC so it will be zeroed
    fd = shm_open(AUDIO_SHM, O_CREAT|O_TRUNC|O_RDWR, 0666);
    if (fd < 0) {
        FATAL("shm_open %s, %s\n", AUDIO_SHM, strerror(errno));
    }
    rc = ftruncate(fd, sizeof(audio_shm_t));
    shm = mmap(NULL, sizeof(audio_shm_t), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (shm == NULL) {
        FATAL("mmap %s, %s\n", AUDIO_SHM, strerror(errno));
    }
    assert(((uintptr_t)shm & (PAGE_SIZE-1)) == 0);

    // don't fork the mmap'ed memory
    rc = madvise(shm, sizeof(audio_shm_t), MADV_DONTFORK);
    if (rc < 0) {
        FATAL("audio madvice(%p,%zd), %s\n", shm, sizeof(audio_shm_t), strerror(errno));
    }

    // init beep_data buffer
    for (int i = MAX_BEEP_DATA/4; i < MAX_BEEP_DATA*3/4; i++) {
        beep_data[i] = BEEP_AMPLITUDE * sin(i * (2*M_PI / MAX_BEEP_DATA * BEEP_FREQUENCY));
    }

    // start the audio pgm;
    // run as root because it sets realtime priority
    rc = system("sudo PA_ALSA_PLUGHW=1 ./audio 2>audio.stderr &");
    if (rc < 0) {
        FATAL("start audio pgm, %s\n", strerror(errno));
    }
    INFO("audio pgm started\n");

    // create the proc_mic_data_thread
    pthread_create(&proc_mic_data_tid, NULL, proc_mic_data_thread, proc_mic_data);

    // register atexit callback
    atexit(audio_exit);
}

static void audio_exit(void)
{
    // wait for proc_mic_data thread to exit
    audio_exitting = true;
    pthread_join(proc_mic_data_tid, NULL);

    // stop audio pgm
    system("sudo killall -SIGTERM audio");
}

static void * proc_mic_data_thread(void *cx)
{
    int curr_fidx;
    int last_fidx = 0;
    int (*proc_mic_data)(short *frame) = cx;

    while (true) {
        if (audio_exitting) {
            break;
        }

        curr_fidx = shm->fidx;
        while (last_fidx != curr_fidx) {
            proc_mic_data(shm->frames[last_fidx]);
            last_fidx = (last_fidx + 1) % 48000;
        }

        usleep(1*MS);
    }

    return NULL;
}

// -----------------  AUDIO IN API ROUTINES  ---------------------------------

int audio_in_reset_mic(void)
{
    int count = 0;

    shm->reset_mic = true;
    while (shm->reset_mic && count++ < 200) {
        usleep(10*MS);
    }

    return (shm->reset_mic == false ? 0 : -1);
}

// -----------------  AUDIO OUT API ROUTINES  --------------------------------

// These 3 routines return when audio output has started.
void audio_out_beep(int beep_count, bool complete_to_idle)
{
    MUTEX_LOCK;
    while (shm->state != AUDIO_OUT_STATE_IDLE) usleep(10*MS);
    shm->state = AUDIO_OUT_STATE_PREP;
    MUTEX_UNLOCK;
    
    shm->sample_rate = BEEP_SAMPLE_RATE;
    for (int i = 0; i < beep_count; i++) {
        memcpy(shm->data + (i * MAX_BEEP_DATA), beep_data, sizeof(beep_data));
    }
    shm->max_data = beep_count * MAX_BEEP_DATA;
    shm->complete_to_idle = complete_to_idle;

    __sync_synchronize();
    shm->state = AUDIO_OUT_STATE_PLAY;
}

void audio_out_play_data(short *data, int max_data, int sample_rate, bool complete_to_idle)
{
    MUTEX_LOCK;
    while (shm->state != AUDIO_OUT_STATE_IDLE) usleep(10*MS);
    shm->state = AUDIO_OUT_STATE_PREP;
    MUTEX_UNLOCK;

    shm->sample_rate = sample_rate;
    memcpy(shm->data, data, max_data*sizeof(short));
    shm->max_data = max_data;
    shm->complete_to_idle = complete_to_idle;

    __sync_synchronize();
    shm->state = AUDIO_OUT_STATE_PLAY;
}

void audio_out_play_wav(char *file_name, short **data, int *max_data, bool complete_to_idle)
{
    int max_chan, rc;

    // wait for AUDIO_OUT_STATE_IDLE, and set state to AUDIO_OUT_STATE_PREP
    MUTEX_LOCK;
    while (shm->state != AUDIO_OUT_STATE_IDLE) usleep(10*MS);
    shm->state = AUDIO_OUT_STATE_PREP;
    MUTEX_UNLOCK;
    
    // read the wav file directly into shm->data
    shm->max_data = sizeof(shm->data)/sizeof(short);
    rc = sf_read_wav_file2(file_name, shm->data, &max_chan, &shm->max_data, &shm->sample_rate);
    if (rc < 0) {
        ERROR("sf_read_wav_file failed, %s\n", file_name);
        return;
    }
    INFO("max_data=%d  max_chan=%d  sample_rate=%d\n", shm->max_data, max_chan, shm->sample_rate);
    assert(shm->max_data > 0 && shm->max_data <= sizeof(shm->data)/sizeof(short));
    assert(max_chan == 1);

    // set the complete_to_idle flag; 
    // this will cause the audio_pgm code, when the audio output has completed, to
    //  set the state to AUDIO_OUT_STATE_IDLE as opposed to AUDIO_OUT_STATE_PLAY_DONE
    shm->complete_to_idle = complete_to_idle;

    // if caller wants copy of data then provide to caller
    if (data) {
        *data = malloc(shm->max_data * sizeof(short));
        memcpy(*data, shm->data, shm->max_data * sizeof(short));
        *max_data = shm->max_data;
    }

    // set AUDIO_OUT_STATE_PLAY
    __sync_synchronize();
    shm->state = AUDIO_OUT_STATE_PLAY;
}

// Wait for audio output to complete.
void audio_out_wait(void)
{
    while (shm->state != AUDIO_OUT_STATE_IDLE && shm->state != AUDIO_OUT_STATE_PLAY_DONE)
        usleep(10*MS);
}

// Return true if audio output has completed (is IDLE).
bool audio_out_is_complete(void)
{
    return (shm->state == AUDIO_OUT_STATE_IDLE || shm->state == AUDIO_OUT_STATE_PLAY_DONE);
}

// Cancel audio output.
void audio_out_cancel(void)
{
    shm->cancel = true;
}

// Set audio output state to AUDIO_OUT_STATE_IDLE. 
// This is intended to be used only after an audio_output that was started with
// the complete_to_idle flag set false, has completed. Calling this routine will
// then allow the next audio output to start.
void audio_out_set_state_idle(void)
{
    if (shm->state != AUDIO_OUT_STATE_PLAY_DONE) {
        ERROR("audio_out_state = %d, should be AUDIO_OUT_STATE_DONE\n", shm->state);
        return;
    }
    shm->state = AUDIO_OUT_STATE_IDLE;
}

// Return sound level for the low, mid and high frequency bands
void audio_out_get_low_mid_high(double *low, double *mid, double *high)
{
    *low  = shm->low;
    *mid  = shm->mid;
    *high = shm->high;
}

// Set audio output volume.
// Notes:
// - aplay -l              - displays card numbers
// - arecord -l            - ditto
// - amixer -c 1 controls  - displays controls
void audio_out_set_volume(int volume)
{
    char cmd[100];
    sprintf(cmd, "amixer -c 1 set PCM Playback Volume %d%%", volume);
    system(cmd);
}
