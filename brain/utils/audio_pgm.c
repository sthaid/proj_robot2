#define _GNU_SOURCE  // needed for sched_setaffinity
#include <utils.h>

// variables
static audio_shm_t *shm;
static bool         end_program;
static pthread_t    recv_mic_data_tid;
static int          recv_mic_data_start_fidx;
static bool         recv_mic_data_workaround;

// prototypes
static void sig_hndlr(int sig);

static void *audio_out_thread(void *cx);
static int audio_out_get_frame(void *data_arg, void *cx);
static void lmh_init(void);
static void lmh(short v);

static int recv_mic_data(const void *frame, void *cx);
static void *recv_mic_data_setup_thread(void *cx);

// -----------------  MAIN  ---------------------------------------------------

int main(int argc, char **argv)
{
    int rc, fd;
    pthread_t audio_out_tid;

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

    // create the audio output thread
    pthread_create(&audio_out_tid, NULL, audio_out_thread, NULL);

    // call pa_record2 to start receiving the 4 channel respeaker mic data;
    // the recv_mic_data callback routine is called with mic data frames
    INFO("AUDIO RUNNING\n");
    while (true) {
        pthread_t tid;

        recv_mic_data_tid = 0;
        recv_mic_data_start_fidx = shm->fidx;
        pthread_create(&tid, NULL, recv_mic_data_setup_thread, NULL);
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

        if (end_program) {
            break;
        }
    }

    // wait for audio output to complete
    pthread_join(audio_out_tid, NULL);

    // terminate
    INFO("AUDIO TERMINATING\n");
    return 0;
}

static void sig_hndlr(int sig)
{
    INFO("audio got %s\n", sig == SIGTERM ? "SIGTERM" : "SIGINT");
    end_program = true;
}

// -----------------  AUDIO OUTPUT  -------------------------------------------

static void *audio_out_thread(void *cx)
{
    int end_program_cnt = 0;

    while (true) {
        // wait
        while (shm->state != AUDIO_OUT_STATE_PLAY) {
            usleep(10*MS);
            if (end_program && end_program_cnt++ > 100) {
                return NULL;
            }
        }

        // play
        shm->cancel = false;
        lmh_init();
        pa_play2("USB", 2, shm->sample_rate, PA_INT16, audio_out_get_frame, NULL);

        // done
        shm->state = (shm->complete_to_idle ? AUDIO_OUT_STATE_IDLE 
                                            : AUDIO_OUT_STATE_PLAY_DONE);
    }

    return NULL;
}

static int audio_out_get_frame(void *data_arg, void *cx)
{
    short *data = data_arg;
    static int data_idx;
    short x;
    int ramp_samples = shm->sample_rate / 10;

    assert(shm->max_data >= 0);

    // if no more data, return -1, causing the call to pa_play2 to return
    if (shm->max_data == 0) {
        return -1;
    }

    // keep track of the amplitude in the low, medium and high frequency bands
    lmh(shm->data[data_idx]);

    // if cancel audio output is requested and we're not already near the end
    // of the audio data the reduce max_data so that just ramp_samples remain
    if (shm->cancel && (data_idx + ramp_samples <= shm->max_data)) {
        shm->max_data = data_idx + ramp_samples;
    }

    // set return data; if data_idx is within ramp_samples of the begining or
    // end of the audio data then ramp the data values up at the begining and 
    // down at the end of the audio data
    if (data_idx < ramp_samples) {
        x = shm->data[data_idx] * ((double)data_idx / ramp_samples);
    } else if (data_idx >= shm->max_data - ramp_samples) {
        x = shm->data[data_idx] * ((double)(shm->max_data-1 - data_idx) / ramp_samples);
    } else {
        x = shm->data[data_idx];
    }
    data[0] = data[1] = x;

    // increment data_idx;
    // if data_idx is now max_dat then reset data_idx and max_data to zero;
    // resetting max_data to zero will cause this routine to return -1 on the next call
    data_idx++;
    if (data_idx >= shm->max_data) {
        data_idx = 0;
        shm->max_data = 0;
    }

    // return 0, indicating that a frame of sound data has been returned
    return 0;
}

// - - - - - - - - - - - 

// this code is used to determine the sound intensity in the 
// low, medium, and high frequency bands

static double cx_low[10], cx_low_smooth;
static double cx_mid[10], cx_mid_smooth;
static double cx_high[10], cx_high_smooth;

static void lmh_init(void)
{
    memset(cx_low, 0, sizeof(cx_low));
    cx_low_smooth = 0;
    memset(cx_mid, 0, sizeof(cx_mid));
    cx_mid_smooth = 0;
    memset(cx_high, 0, sizeof(cx_high));
    cx_high_smooth = 0;
}

static void lmh(short v)
{
    #define SMOOTH 0.995
    double low, high, mid;

    low = low_pass_filter_ex(v, cx_low, 7, .85);
    shm->low = low_pass_filter(fabs(low), &cx_low_smooth, SMOOTH);

    high = high_pass_filter_ex(v, cx_high, 5, .85);
    shm->high = low_pass_filter(fabs(high), &cx_high_smooth, SMOOTH);

    mid = high_pass_filter_ex(low, cx_mid, 5, .85);
    shm->mid = low_pass_filter(fabs(mid), &cx_mid_smooth, SMOOTH);
}

// -----------------  AUDIO INPUT FROM RESPEAKER 4 CHAN MIC  ------------------

static int recv_mic_data(const void *frame_arg_as_void, void *cx)
{
    static int cnt;
    static int cnt2;
    static short frame_last[4];

    const short *frame_arg = frame_arg_as_void;

    // get this thread id, for use by recv_mic_data_setup_thread
    if (recv_mic_data_tid == 0) {
        recv_mic_data_tid = pthread_self();
    }

    // check if this program is terminating; if so,
    // return -1 to stop receiving mic data, and pa_record2 will return
    if (end_program) {
        return -1;
    }

    // if requested to reset the mic then return -1 to 
    // stop receiving mic data, and cause the call to pa_record2 to return
    if (shm->reset_mic) {
        shm->reset_mic = false;

        recv_mic_data_workaround = false;
        cnt = 0;
        cnt2 = 0;
        memset(frame_last, 0, sizeof(frame_last));

        return -1;
    }

    // when starting to receive mic data we can detect if workaround is needed
    // by checking initial frames for mic data values where mic 0 and 1 have zero
    // values and mic 2 & 3 are non zero
    if (cnt2 < 10) {
        if (frame_arg[0] == 0 && frame_arg[1] == 0 && frame_arg[2] != 0 && frame_arg[3] != 0) {
            recv_mic_data_workaround = true;
        }
        cnt2++;
    }

    // store frame in array, to be processed by the proc_mic_data_thread, in brain.c
    if (recv_mic_data_workaround == false) {
        memcpy(shm->frames[shm->fidx+cnt], frame_arg, sizeof(shm->frames[0]));
    } else {
        short *frame = shm->frames[shm->fidx+cnt];
        frame[0] = frame_last[2];        
        frame[1] = frame_last[3];        
        frame[2] = frame_arg[0];
        frame[3] = frame_arg[1];
        memcpy(frame_last, frame_arg, 8);
    }
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

static void *recv_mic_data_setup_thread(void *cx)
{
    while (recv_mic_data_tid == 0) {
        usleep(10*MS);
    }

    struct sched_param param;
    cpu_set_t cpu_set;
    int rc;

    INFO("audio setting recv_mic_data realtime & affinity\n");

    // set affinity to cpu 3
    // notes: 
    // - to isolate a cpu
    //     add isolcpus=3 to /boot/cmdline, and reboot
    // - to verify cpu has been isolated
    //     cat /sys/devices/system/cpu/isolated
    //     cat /sys/devices/system/cpu/present
    CPU_ZERO(&cpu_set);
    CPU_SET(3, &cpu_set);
    rc = pthread_setaffinity_np(recv_mic_data_tid,sizeof(cpu_set_t),&cpu_set);
    if (rc < 0) {
        FATAL("audio sched_setaffinity, %s\n", strerror(errno));
    }

    // set realtime priority
    // notes:
    // - to verify
    //      ps -eLo rtprio,comm  | grep audio
    memset(&param, 0, sizeof(param));
    param.sched_priority = 95;
    rc = pthread_setschedparam(recv_mic_data_tid, SCHED_FIFO, &param);
    if (rc < 0) {
        FATAL("audio sched_setscheduler, %s\n", strerror(errno));
    }

    // print first few frames, for debug
    int i, start, end;
    while (recv_mic_data_start_fidx == shm->fidx) {
        usleep(10*MS);
    }
    start = recv_mic_data_start_fidx;
    end   = recv_mic_data_start_fidx + 10;
    INFO("WORKAROUND = %d\n", recv_mic_data_workaround);
    for (i = start; i < end; i++) {
        short *frame = shm->frames[i];
        INFO("first mic frames %d: %6d %6d %6d %6d\n",
             i-start, frame[0], frame[1], frame[2], frame[3]);
    }

    // terminate thread
    return NULL;
}
