#include "common.h"

//
// defines
//

#define EMER_STOP_THREAD_DISABLED            0
#define EMER_STOP_THREAD_ENABLED             1
#define EMER_STOP_THREAD_DISABLED_OCCURRED   2

#define EMER_STOP_OCCURRED (emer_stop_thread_state == EMER_STOP_THREAD_DISABLED_OCCURRED)

//
// variables
//

static int (*drive_proc)(void);

static struct {
    int    speed;
    double left_mph;
    double right_mph;
} drive_cal_tbl[32];

static int emer_stop_thread_state;

//
// prototypes
//

static int drive_cal_file_read(void);
static int drive_cal_file_write(void);
static void drive_cal_tbl_print(void);
static int drive_cal_execute(void);
static int drive_cal_cvt_mph_to_left_motor_speed(double mph, int *left_mtr_speed);
static int drive_cal_cvt_mph_to_right_motor_speed(double mph, int *right_mtr_speed);

static int drive_sleep(uint64_t duration_us);

static void *drive_thread(void *cx);
static void *emer_stop_thread(void *cx);
static void emer_stop_button_cb(int id, bool pressed, int pressed_duration_us);

// -----------------  API  --------------------------------------------------

int drive_init(void)
{
    pthread_t tid;

    // register for left-button press, this will trigger an emergency stop
    button_register_cb(0, emer_stop_button_cb);

    // read the drive.cal file
    drive_cal_file_read();
    drive_cal_tbl_print();

    // create drive_thread and emer_stop_thread
    pthread_create(&tid, NULL, emer_stop_thread, NULL);
    pthread_create(&tid, NULL, drive_thread, NULL);
    
    // success
    return 0;
}

void drive_run_cal(void)
{
    if (drive_proc) {
        ERROR("drive_proc is currently running\n");
        return;
    }

    drive_proc = drive_cal_execute;
}

void drive_run_proc(int proc_id)
{
    int (*proc)(void) = drive_procs_tbl[proc_id];

    if (proc == NULL) {
        ERROR("invalid proc_id %d\n", proc_id);
    }

    if (drive_proc) {
        ERROR("drive_proc is currently running\n");
        return;
    }

    drive_proc = proc;
}

// -----------------  DRIVE CALIBRATION  -------------------------------------

static int drive_cal_file_read(void)
{
    int fd, len;

    fd = open("drive.cal", O_RDONLY);
    if (fd < 0) {
        ERROR("failed to open drive.cal, %s\n", strerror(errno));
        return -1;
    }

    if ((len = read(fd, drive_cal_tbl, sizeof(drive_cal_tbl))) != sizeof(drive_cal_tbl)) {
        ERROR("failed to read drive.cal, len=%d, %s\n", len, strerror(errno));
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

static int drive_cal_file_write(void)
{
    int fd, len;

    fd = open("drive.cal", O_CREAT|O_TRUNC|O_WRONLY, 0666);
    if (fd < 0) {
        ERROR("failed to open drive.cal, %s\n", strerror(errno));
        return -1;
    }

    if ((len = write(fd, drive_cal_tbl, sizeof(drive_cal_tbl))) != sizeof(drive_cal_tbl)) {
        ERROR("failed to write drive.cal, len=%d, %s\n", len, strerror(errno));
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

static void drive_cal_tbl_print(void)
{
    if (drive_cal_tbl[0].speed == 0) {
        WARN("no drive_cal_tbl\n");
        return;
    }

    INFO("     SPEED   LEFT_MPH  RIGHT_MPH\n");
    for (int i = 0; drive_cal_tbl[i].speed; i++) {
        INFO("%d  %10d %10.2f %10.2f\n",
            i, drive_cal_tbl[i].speed, drive_cal_tbl[i].left_mph, drive_cal_tbl[i].right_mph);
    }
}

static int drive_cal_execute(void)
{
    static int speed_tbl[] = {-2000, -500, 500, 2000};

    #define MAX_SPEED_TBL (sizeof(speed_tbl)/sizeof(speed_tbl[0]))
    #define CAL_INTVL_US  (60 * 1000000)

    for (int i = 0; i < MAX_SPEED_TBL; i++) {
        uint64_t start_us, duration_us;
        int speed = speed_tbl[i];
        int left_enc, right_enc;

        // set both motors to speed, and 
        // wait a few secs to stabilize
        if (abs(speed) <= 500) {  // xxx temporary ?
            mc_set_speed_all(1000,1000);
            if (drive_sleep(1000000) < 0) {
                return -1;
            }
        }
        mc_set_speed_all(speed,speed);
        if (drive_sleep(3000000) < 0) {
            return -1;
        }

        // reset encoder positions
        // sleep for INTVL_US
        // read encoder positions
        start_us = microsec_timer();
        encoder_pos_reset(0);
        encoder_pos_reset(1);
        if (drive_sleep(CAL_INTVL_US) < 0)  {
            return -1;
        }
        left_enc = encoder_get_position(0);
        right_enc = encoder_get_position(1);
        duration_us = microsec_timer() - start_us;

        // save results in drive_cal_tbl
        drive_cal_tbl[i].speed     = speed;
        drive_cal_tbl[i].left_mph  = (( left_enc/979.62) * (.080*M_PI) / (duration_us/1000000.)) * 2.23694;
        drive_cal_tbl[i].right_mph = ((right_enc/979.62) * (.080*M_PI) / (duration_us/1000000.)) * 2.23694;
    }

    // debug print the cal_tbl
    drive_cal_tbl_print();

    // write cal_tbl to file
    drive_cal_file_write();

    // success
    return 0;
}

static int drive_cal_cvt_mph_to_left_motor_speed(double mph, int *left_mtr_speed)
{
    *left_mtr_speed = 0;
    for (int i = 0; drive_cal_tbl[i+1].speed; i++) {
        if (mph >= drive_cal_tbl[i].left_mph && mph <= drive_cal_tbl[i+1].left_mph) {
            *left_mtr_speed = nearbyint(
               drive_cal_tbl[i].speed + 
               (drive_cal_tbl[i+1].speed - drive_cal_tbl[i].speed) *
               ((mph - drive_cal_tbl[i].left_mph) / 
                (drive_cal_tbl[i+1].left_mph - drive_cal_tbl[i].left_mph))
                                            );
            return 0;
        }
    }
    return -1;
}

static int drive_cal_cvt_mph_to_right_motor_speed(double mph, int *right_mtr_speed)
{
    *right_mtr_speed = 0;
    for (int i = 0; drive_cal_tbl[i+1].speed; i++) {
        if (mph >= drive_cal_tbl[i].right_mph && mph <= drive_cal_tbl[i+1].right_mph) {
            *right_mtr_speed = nearbyint(
               drive_cal_tbl[i].speed + 
               (drive_cal_tbl[i+1].speed - drive_cal_tbl[i].speed) *
               ((mph - drive_cal_tbl[i].right_mph) / 
                (drive_cal_tbl[i+1].right_mph - drive_cal_tbl[i].right_mph))
                                            );
            return 0;
        }
    }
    return -1;
}

// -----------------  DRIVE PROCS SUPPORT ROUTINES  -------------------------

// XXX update this routine to drive fwd or rev,  or make 2 new routines
int drive_fwd(double secs, double mph)
{
    int left_speed, right_speed;

    // convert the mph to left/right mtr ctlr speed
    if (drive_cal_cvt_mph_to_left_motor_speed(mph, &left_speed) < 0) {
        ERROR("failed to get left_speed for %0.1f mph\n", mph);
        return -1;
    }
    if (drive_cal_cvt_mph_to_right_motor_speed(mph, &right_speed) < 0) {
        ERROR("failed to get right_speed for %0.1f mph\n", mph);
        return -1;
    }
    INFO("secs=%0.1f mph=%0.2f speed=%d %d\n", secs, mph, left_speed, right_speed);

    // enable fwd proximity sensor
    proximity_enable(0);
    proximity_disable(1);

    // start motors
    if (mc_set_speed_all(left_speed, right_speed) < 0) {
        return -1;
    }

    // monitor for completion or emergency stop error
    if (drive_sleep(secs) < 0) {
        return -1;
    }

    // success
    return 0;
}

int drive_stop(void)
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
        if (EMER_STOP_OCCURRED) {
            return -1;
        }
        if (microsec_timer()-start_us > 1000000) {  // 1sec
            return -1;
        }
        if (encoder_get_speed(0) == 0 && encoder_get_speed(1) == 0) {
            INFO("stepped after %lld us\n", microsec_timer()-start_us);
            break;
        }
        usleep(10000);  // 10 ms
    }

    // success
    return 0;
}

static int drive_sleep(uint64_t duration_us)
{
    uint64_t done_us = microsec_timer() + duration_us;

    while (true) {
        if (EMER_STOP_OCCURRED) {
            return -1;
        }
        if (microsec_timer() > done_us) {
            return 0;
        }
        usleep(10000);  // 10 ms
    }
}

// -----------------  THREADS  ----------------------------------------------

// - - - - - - - - -  DRIVE_THREAD   - - - - - - - - - - - - - - - - - 

static void *drive_thread(void *cx)
{
    struct sched_param param;
    int rc;

    // set realtime priority
    memset(&param, 0, sizeof(param));
    param.sched_priority = 80;
    rc = sched_setscheduler(0, SCHED_FIFO, &param);
    if (rc < 0) {
        FATAL("sched_setscheduler, %s\n", strerror(errno));
    }

    while (true) {
        // wait for a request 
        while (drive_proc == NULL) {
            usleep(10000);  // 10 ms
        }

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

        // enable emer_stop_thread
        emer_stop_thread_state = EMER_STOP_THREAD_ENABLED;

        // call the drive proc
        drive_proc();

        // disable emer_stop_thread
        if (emer_stop_thread_state == EMER_STOP_THREAD_ENABLED) {
            emer_stop_thread_state = EMER_STOP_THREAD_DISABLED;
        }

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

// - - - - - - - - -  EMER_STOP_THREAD - - - - - - - - - - - - - - - - 

static uint64_t enc_low_speed_start_time[2];
static bool     stop_button_pressed;

static void *emer_stop_thread(void *cx)
{
    mc_status_t *mcs = mc_get_status();
    int enc_left_errs, enc_right_errs;
    bool prox_front, prox_rear;
    double accel;
    struct sched_param param;
    int rc;

    #define DO_EMER_STOP(fmt, args...) \
        do { \
            ERROR(fmt, ## args); \
            mc_disable_all(); \
            emer_stop_thread_state = EMER_STOP_THREAD_DISABLED_OCCURRED; \
            goto emer_stopped; \
        } while (0)


    // set realtime priority
    memset(&param, 0, sizeof(param));
    param.sched_priority = 80;
    rc = sched_setscheduler(0, SCHED_FIFO, &param);
    if (rc < 0) {
        FATAL("sched_setscheduler, %s\n", strerror(errno));
    }

    while (true) {
emer_stopped:
        while (emer_stop_thread_state != EMER_STOP_THREAD_ENABLED) {
            enc_low_speed_start_time[0] = 0;
            enc_low_speed_start_time[1] = 0;
            stop_button_pressed = false;
            usleep(10000);  // 10 ms
        }

        if (mcs->state == MC_STATE_DISABLED) {
            DO_EMER_STOP("mc-disabled\n");
        }

        if (stop_button_pressed) {
            DO_EMER_STOP("stop-button\n");
        }

        enc_left_errs = enc_right_errs = 0;
        if ((enc_left_errs = encoder_get_errors(0)) || (enc_right_errs = encoder_get_errors(1))) {
            DO_EMER_STOP("enc-errors-%d-%d\n", enc_left_errs, enc_right_errs);
        }

        prox_front = prox_rear = false;
        if ((prox_front = proximity_check(0,NULL)) || (prox_rear = proximity_check(1,NULL))) {
            DO_EMER_STOP("proximity-%d-%d\n", prox_front, prox_rear);
        }

        if (imu_check_accel_alert(&accel)) {
            DO_EMER_STOP("accel=%0.1f\n", accel);
        }

        for (int id = 0; id < 2; id++) {
            int actual_speed = encoder_get_speed(id);
            int expected_speed = (8600./3200.) * mcs->target_speed[id];  // XXX  defines, or drive cal
            if (actual_speed < expected_speed * 0.5) {  // xxx handle negative
                if (enc_low_speed_start_time[id] == 0) {
                    enc_low_speed_start_time[id] = microsec_timer();
                }
            } else {
                enc_low_speed_start_time[id] = 0;
            }
            if (enc_low_speed_start_time[id] != 0 &&
                microsec_timer() - enc_low_speed_start_time[id] > 500000)   // 500 ms
            {
                DO_EMER_STOP("enc-%d speed actual=%d exp=%d\n", id, actual_speed, expected_speed);
            }
        }

        usleep(10000);  // 10 ms
    }

    return NULL;
}

static void emer_stop_button_cb(int id, bool pressed, int pressed_duration_us)
{
    if (pressed) {
        stop_button_pressed = true;
    }
}
