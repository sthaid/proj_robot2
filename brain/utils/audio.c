#include <utils.h>

static void audio_exit(void);

// --------------------------------------------------------------------------

void audio_init(void)
{
    int rc, fd;

    // if audio pgm is already running, then error
    rc = system("pgrep -x audio");
    if (rc == 0) {
        // xxx maybe kill it here if it is
        FATAL("audio pgm is already running\n");
    }
    INFO("audio pgm is not running\n");

    // create and map audio_shm
    fd = shm_open(AUDIO_SHM, O_CREAT|O_TRUNC|O_RDWR, 0666);
    if (fd < 0) {
        FATAL("shm_open %s, %s\n", AUDIO_SHM, strerror(errno));
    }
    rc = ftruncate(fd, sizeof(audio_shm_t));
    shm = mmap(NULL, sizeof(audio_shm_t), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (shm == NULL) {
        FATAL("mmap %s, %s\n", AUDIO_SHM, strerror(errno));
    }

    INFO("shm %p\n", shm);
    // xxx is it zero

    // start the audio pgm
    rc = system("sudo ./audio &");
    if (rc < 0) {
        FATAL("start audio pgm, %s\n", strerror(errno));
    }
    INFO("audio pgm started\n");
    sleep(1);

    // create thread to process mic data
    for (int i = 0; i < 10; i++) {
        INFO("%d\n", shm->fidx);
        usleep(100000);
    }

    // atexit, stop the audio pgm
    atexit(audio_exit);
}

static void audio_exit(void)
{
    static bool called = false;

    // if already called then return
    if (called) {
        return;
    }
    called = true;

    //while (shm->execute) {
        //usleep(1000);
    //}

    // stop audio pgm
    INFO("*** KILL AUDIO\n");
    system("sudo killall -SIGTERM audio");
    INFO("*** AFTER KILL AUDIO\n");
}

// --------------------------------------------------------------------------

void audio_out_beep(int beep_count)
{
    while (shm->execute) {
        usleep(1000);
    }

    shm->beep_count = beep_count;
    __sync_synchronize();

    shm->execute = true;
    __sync_synchronize();
}

void audio_out_play(short *data, int max_data)
{
    while (shm->execute) {
        usleep(1000);
    }

    // xxx try to avoid this copy
    memcpy(shm->data, data, max_data*sizeof(short));
    shm->max_data = max_data;
    __sync_synchronize();

    shm->execute = true;
    __sync_synchronize();
}

