#include "common.h"

//
// defines
//

//
// variables
//

static int (*drive_proc)(void);

static bool emer_stop_button;
static bool emer_stop_sigint;
static pthread_t emer_stop_thread_id;

//
// prototypes
//

static int drive_proc_1(void);

static int drive_fwd(double secs, int speed);
static int drive_stop(void);
static void *drive_thread(void *cx);

static int check_emer_stop(bool first_call);

static void emer_stop_button_cb(int id, bool pressed, int pressed_duration_us);
static void emer_stop_sig_hndlr(int sig);
static void *emer_stop_thread(void *cx);

// -----------------  API  --------------------------------------------------

int drive_init(void)
{
    pthread_t tid;
    struct sigaction act;

    // init to support triggering an emergency stop via ctrl-c or by button press:
    // XXX note that when this is running as a service the ctrl-c may no 
    //     longer be appropriate
    // - create emer_stop_thread, this thread is signalled when a ctrl-c or 
    //   button press, when signalled this thread will call mc_disable_all
    pthread_create(&emer_stop_thread_id, NULL, emer_stop_thread, NULL);
    // - register for SIGINT and SIGUSR1
    memset(&act, 0, sizeof(act));
    act.sa_handler = emer_stop_sig_hndlr;
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGUSR1, &act, NULL);
    // - register for left-button press
    button_register_cb(0, emer_stop_button_cb);

    // create drive_thread
    pthread_create(&tid, NULL, drive_thread, NULL);
    
    // success
    return 0;
}

void drive_run(void)
{
    if (drive_proc) {
        ERROR("drive_proc is currently running\n");
        return;
    }

    drive_proc = drive_proc_1;
}

// -----------------  DRIVE PROCS  ------------------------------------------

static int drive_proc_1(void)
{
#if 0
    if (drive_fwd(10, 1500) < 0) return -1;
    if (drive_fwd(10, 1000) < 0) return -1;
    if (drive_fwd(10, 700) < 0) return -1;
    if (drive_fwd(10, 500) < 0) return -1;
    if (drive_fwd(10, 300) < 0) return -1;
    if (drive_fwd(10, 100) < 0) return -1;
#else
    if (drive_fwd(60, 1000) < 0) return -1;
#endif

    if (drive_stop() < 0) return -1;

    return 0;
}

// -----------------  DRIVE PROCS SUPPORT ROUTINES  -------------------------

static int drive_fwd(double secs, int speed)
{
    uint64_t done_us;

    INFO("secs=%0.1f speed=%d\n", secs, speed);

    // enable fwd proximity sensor
    proximity_enable(0);
    proximity_disable(1);

    // start motors
    if (mc_set_speed_all(speed, speed) < 0) {
        return -1;
    }

    // monitor for completion or emergency stop error
    done_us = microsec_timer() + secs * MS2US(1000);
    while (true) {
        if (check_emer_stop(false)) {
            return -1;
        }
        if (microsec_timer() > done_us) {
            break;
        }
        usleep(MS2US(10));
    }

    // success
    return 0;
}

static int drive_stop(void)
{
    uint64_t start_us;

    INFO("called\n");

    // disable proximty sensors
    proximity_disable(0);
    proximity_disable(1);

    // set all motor speeds to 0
    if (mc_set_speed_all(0,0) < 0) {
        return -1;
    }

    // wait up to 1 second for encoder speed to drop to 0
    start_us = microsec_timer();
    while (true) {
        if (check_emer_stop(false) || (microsec_timer()-start_us > MS2US(1000))) {
            return -1;
        }
        if (encoder_get_speed(0) == 0 && encoder_get_speed(1) == 0) {
            INFO("stepped after %lld us\n", microsec_timer()-start_us);
            break;
        }
        usleep(MS2US(10));
    }

    // success
    return 0;
}

static void *drive_thread(void *cx)
{
    struct sched_param param;
    int rc;

    // set realtime priority
    memset(&param, 0, sizeof(param));
    param.sched_priority = 85;
    rc = sched_setscheduler(0, SCHED_FIFO, &param);
    if (rc < 0) {
        FATAL("sched_setscheduler, %s\n", strerror(errno));
    }

    while (true) {
        // wait for a request 
        while (drive_proc == NULL) {
            usleep(MS2US(10));
        }

        // clear the emer_stop flags
        emer_stop_sigint = false;
        emer_stop_button = false;

        // enable motor-ctlr and sensors
        if (mc_enable_all() < 0) {
            drive_proc = NULL;
            continue;
        }
        encoder_enable(0);
        encoder_enable(1);
        proximity_disable(0);
        proximity_disable(1);
        imu_accel_enable();

        // make an initial check for any errors prior to calling drive_prox,
        // the drive_proc also checks for errors as part of its processing
        if (check_emer_stop(true) < 0) {
            goto finished;
        }

        // call the drive proc
        drive_proc();

finished:
        // disable motor-ctlr and sensors
        mc_disable_all();
        encoder_disable(0);
        encoder_disable(1);
        proximity_disable(0);
        proximity_disable(1);
        imu_accel_disable();

        // clear drive_proc
        drive_proc = NULL;
    }

    return NULL;
}

// -----------------  EMERGENCY STOP  ---------------------------------------

static int check_emer_stop(bool first_call)
{
    mc_status_t *mcs = mc_get_status();
    int enc_left_errs=0, enc_right_errs=0;
    bool prox_front=false, prox_rear=false;
    double accel;

    static struct {
        int actual_speed;
        int expected_speed;
        uint64_t low_speed_start_time;
    } enc[2];

    if (first_call) {
        memset(enc, 0, sizeof(enc));
    }

    if (mcs->state == MC_STATE_DISABLED) {
        ERROR("mc-disabled\n");
        return -1;
    }

    if (emer_stop_sigint) {
        ERROR("ctrl-c\n");
        mc_disable_all();
        return -1;
    }

    if (emer_stop_button) {
        ERROR("stop-button\n");
        mc_disable_all();
        return -1;
    }

    if ((enc_left_errs = encoder_get_errors(0)) || (enc_right_errs = encoder_get_errors(1))) {
        ERROR("enc-errors-%d-%d\n", enc_left_errs, enc_right_errs);
        mc_disable_all();
        return -1;
    }

    if ((prox_front = proximity_check(0,NULL)) || (prox_rear = proximity_check(1,NULL))) {
        ERROR("proximity-%d-%d\n", prox_front, prox_rear);
        mc_disable_all();
        return -1;
    }

    if (imu_check_accel_alert(&accel)) {
        ERROR("accel=%0.1f\n", accel);
        mc_disable_all();
        return -1;
    }

    for (int id = 0; id < 2; id++) {
        enc[id].actual_speed = encoder_get_speed(id);
        enc[id].expected_speed = (8600./3200.) * mcs->target_speed[id];
        if (enc[id].actual_speed < enc[id].expected_speed * 0.5) {
            if (enc[id].low_speed_start_time == 0) {
                enc[id].low_speed_start_time = microsec_timer();
            }
        } else {
            enc[id].low_speed_start_time = 0;
        }
        if (enc[id].low_speed_start_time != 0 &&
            microsec_timer() - enc[id].low_speed_start_time > MS2US(500)) 
        {
            ERROR("enc-%d speed actual=%d exp=%d\n", id, enc[id].actual_speed, enc[id].expected_speed);
            mc_disable_all();
            return -1;
        }
    }
    
    return 0;
}

static void emer_stop_button_cb(int id, bool pressed, int pressed_duration_us)
{
    if (!pressed) {
        return;
    }

    pthread_kill(emer_stop_thread_id, SIGUSR1);
    emer_stop_button = true;
}

static void emer_stop_sig_hndlr(int sig)
{
    switch (sig) {
    case SIGINT:
        pthread_kill(emer_stop_thread_id, SIGUSR1);
        emer_stop_sigint = true;
        break;
    case SIGUSR1:
        break;
    }
}

static void *emer_stop_thread(void *cx)
{
    while (true) {
        pause();
        INFO("calling mc_disable_all\n");
        mc_disable_all();
    }

    return NULL;
}
