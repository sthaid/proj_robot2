#include <utils.h>

// variables
static pthread_t proc_mic_data_tid;
static audio_shm_t *shm;
static bool audio_exitting;

// prototypes
static void audio_exit(void);
static void *proc_mic_data_thread(void *cx);

// -----------------  INIT  -------------------------------------------------

void audio_init(int (*proc_mic_data)(short *frame))
{
    int rc, fd;

    // if audio pgm is already running, then error
    rc = system("pgrep -x audio");
    if (rc == 0) {
        FATAL("audio pgm is already running\n");
    }

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

// -----------------  PROC_MIC_DATA_THREAD  ---------------------------------

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

        usleep(1000);
    }

    return NULL;
}

// -----------------  API ROUTINES  -----------------------------------------

// these routines issue requests to the audio_pgm 

void audio_out_beep(int beep_count)
{
    shm->beep_count = beep_count;
    __sync_synchronize();
}

void audio_out_play_data(short *data, int max_data)
{
    while (shm->max_data) {
        usleep(1000);
    }

    memcpy(shm->data, data, max_data*sizeof(short));
    __sync_synchronize();
    shm->max_data = max_data;
    __sync_synchronize();
}

void audio_out_play_wav(char *file_name, short **data, int *max_data_arg)
{
    int max_data, max_chan, sample_rate, rc;
    
    // wait for in progress audio output to complete
    while (shm->max_data) {
        usleep(1000);
    }

    // read the wav file directly into shm->data
    max_data = sizeof(shm->data)/sizeof(short);
    rc = sf_read_wav_file2(file_name, shm->data, &max_chan, &max_data, &sample_rate);
    if (rc < 0) {
        ERROR("sf_read_wav_file failed\n");
        return;
    }
    INFO("max_data=%d  max_chan=%d  sample_rate=%d\n", max_data, max_chan, sample_rate);
    assert(max_data > 0 && max_data <= sizeof(shm->data)/sizeof(short));
    assert(max_chan == 1);
    assert(sample_rate == 24000);

    // if caller wants copy of data then provide to caller
    if (data) {
        *data = malloc(max_data * sizeof(short));
        memcpy(*data, shm->data, max_data * sizeof(short));
        *max_data_arg = max_data;
    }

    // set shm->max_data, this will cause audio_pgm to play the shm->data
    __sync_synchronize();
    shm->max_data = max_data;
    __sync_synchronize();
}
