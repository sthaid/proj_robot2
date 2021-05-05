#include "common.h"

//
// defines
//

#define EMER_STOP_THREAD_DISABLED  0
#define EMER_STOP_THREAD_ENABLED   1

#define EMER_STOP_OCCURRED (emer_stop_thread_state == EMER_STOP_THREAD_DISABLED)

#define MIN_MTR_SPEED 700   // xxx cal starting here

#define ENC_POS_TO_FEET(pos)  ((pos) * ((1/979.62) * (.080*M_PI) * 3.28084))

//
// variables
//

static int (*drive_proc)(void);

static int emer_stop_thread_state;

//
// prototypes
//

static int drive_common(double lmph, double rmph, double feet);

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

void drive_run(int proc_id)
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

// this is public because it is called from drive_cal.c,
// there should not be a need to call this from drive_procs.c
int drive_sleep(uint64_t duration_us)
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

void drive_emer_stop(void)
{
    ERROR("emergency stop\n");
    mc_disable_all();
}

// -----------------  DRIVE PROCS SUPPORT ROUTINES  -------------------------

static int drive_common(double lmph, double rmph, double feet)
{
    // The larger of lmph or rmph will be the reference motor. The other 
    // motor will sync its distance to the reference motor.
    //
    // The mph values will only be used when determining the initial motor
    // speed. And for the desired ratio of the encoder speeds. The speed ratio is
    // used to determine the target distance for the syncing motor.
    //
    // After the initial acceleration completes the process of syncing to the
    // reference motor will start. Every 100 ms:
    // - the predicted future position of the reference motor is calculated
    //   (for 500 ms in future)
    // - the desired future position of the syncing motor is calculaed;
    //   by multiplying the predicted future position of the reference motor 
    //   by the speed ratio
    // - the predicted future position of the syncing motor is calculated
    // - if the predicted and desired future syncing motor positions are close
    //   then no motor speed adjustment is made; otherwise the speed of the 
    //   syncing motor is increased or decreased by 1

    // The turn radius of a point at the center of the wheelbase is:
    //
    //                R + W/2
    //     Vratio =  ----------
    //                R - W/2
    //
    //                 Vratio + 1
    //      R = (W/2) ------------
    //                 Vratio - 1
    //
    //      Vratio      Radius
    //      ------      ------ 
    //      infinite    0.50 W   (one wheel has zero velocity)
    //        4         0.83 W   
    //        3         1.00 W
    //        2         1.50 W
    //        1.5       2.50 W
    //        1.25      4.50 W
    //        1         infinite  (straight line)
    //       -1         0         (spinning)

    // xxx clean up
    int left_mtr_speed, right_mtr_speed;
    int ref_mtr_id, ref_mtr_speed;
    int sync_mtr_id, sync_mtr_speed;

    int last_ref_enc_val, last_sync_enc_val;
    uint64_t last_enc_val_time;
    int ms;

    int curr_ref_enc_val, curr_sync_enc_val;
    int ref_enc_rate, sync_enc_rate;
    //int accel_intvl_ms;

    int predicted_future_ref_enc_val;
    int predicted_future_sync_enc_val;
    int desired_future_sync_enc_val;
    double ref_mtr_mph, sync_mtr_mph;

    double ref_mtr_feet_travelled;

    // convert the mph to left/right mtr ctlr speed
    if (drive_cal_cvt_mph_to_left_motor_speed(lmph, &left_mtr_speed) < 0) {
        ERROR("failed to get left_speed for %0.1f mph\n", lmph);
        return -1;
    }
    if (drive_cal_cvt_mph_to_right_motor_speed(rmph, &right_mtr_speed) < 0) {
        ERROR("failed to get right_speed for %0.1f mph\n", rmph);
        return -1;
    }

    // init variables relating to the reference and syncing mtrs
    if (fabs(lmph) >= fabs(rmph)) {
        ref_mtr_id     = 0;  // left mtr
        ref_mtr_mph    = lmph; // xxx name
        ref_mtr_speed  = left_mtr_speed;
        sync_mtr_id    = 1;  // right mtr
        sync_mtr_mph   = rmph;
        sync_mtr_speed = right_mtr_speed;
    } else {
        ref_mtr_id     = 1;  // right mtr
        ref_mtr_mph    = rmph;
        ref_mtr_speed  = right_mtr_speed;
        sync_mtr_id    = 0;  // left mtr
        sync_mtr_mph   = lmph;
        sync_mtr_speed = left_mtr_speed;
    }

    // validate that the mtr speeds exceed the minimum allowed
    if (abs(ref_mtr_speed) < MIN_MTR_SPEED || abs(sync_mtr_speed) < MIN_MTR_SPEED) {
        ERROR("speed too low %d %d\n", ref_mtr_speed, sync_mtr_speed);
        return -1;
    }

    // reset encoders
    encoder_pos_reset(0);
    encoder_pos_reset(1);

    // get last enc values and current time
    last_ref_enc_val = encoder_get_position(ref_mtr_id);
    last_sync_enc_val = encoder_get_position(sync_mtr_id);
    last_enc_val_time = microsec_timer();

    // enable or disabled the proximity sensors based on whether the
    // robot is spinning, moving forward, or moving reverse
    if (ref_mtr_mph * sync_mtr_mph < 0) {
        // spinning
        proximity_enable(0);   // enable front
        proximity_enable(1);   // enable rear
    } else if (ref_mtr_mph > 0) {
        // moving forward
        proximity_enable(0);   // enable front
        proximity_disable(1);  // disable rear
    } else {
        // moving reverse
        proximity_disable(0);  // disable front
        proximity_enable(1);   // enable rear
    }

    // start motors
    INFO("set speed %d %d\n", left_mtr_speed, right_mtr_speed);
    if (mc_set_speed_all(left_mtr_speed, right_mtr_speed) < 0) {
        return -1;
    }

    // xxx do distance later
    ms = 0;
    while (true) {
        // check if the emer_stop_thread shut down the motors
        if (EMER_STOP_OCCURRED) {
            return -1;
        }

        // if the ref mtr has travelled the desired number of feet then
        // break (return)
        ref_mtr_feet_travelled = ENC_POS_TO_FEET(encoder_get_position(ref_mtr_id));
        if (fabs(ref_mtr_feet_travelled) > feet) {
            break;
        }

        // every 100 ms check for the need to adjust the sync mtr speed
        // so that the sync mtr's encoder position tracks the ref mtr's
        // encoder position 
        if (ms != 0 && ((ms % 100) == 0)) do {
            uint64_t time_now = microsec_timer();
            
            // get current encoder values;
            // calculate encoder rates;
            // save last encoder values and last time
            curr_ref_enc_val = encoder_get_position(ref_mtr_id);
            curr_sync_enc_val = encoder_get_position(sync_mtr_id);

            ref_enc_rate = 1000000LL * (curr_ref_enc_val - last_ref_enc_val) / (time_now - last_enc_val_time);
            sync_enc_rate = 1000000LL * (curr_sync_enc_val - last_sync_enc_val) / (time_now - last_enc_val_time);

            last_ref_enc_val  = curr_ref_enc_val;
            last_sync_enc_val = curr_sync_enc_val;
            last_enc_val_time = time_now;

            // calc the predicted and desired encoder values for 0.5 secs in the future
            predicted_future_ref_enc_val  = curr_ref_enc_val + 0.5 * ref_enc_rate;
            predicted_future_sync_enc_val = curr_sync_enc_val + 0.5 * sync_enc_rate;
            desired_future_sync_enc_val   = predicted_future_ref_enc_val * (sync_mtr_mph / ref_mtr_mph);

            // adjust ths sync_mtr_speed, if necessary, based on the sync mtr's 
            // desired and predicted encoder positions in 0.5 secs on the future
            int sync_mtr_speed_adj = (desired_future_sync_enc_val - predicted_future_sync_enc_val) / 50;
            if (sync_mtr_speed_adj) {
                if (sync_mtr_speed_adj > 3) sync_mtr_speed_adj = 3;   // xxx is this needed
                if (sync_mtr_speed_adj < -3) sync_mtr_speed_adj = -3;
                sync_mtr_speed += sync_mtr_speed_adj;
                mc_set_speed(sync_mtr_id, sync_mtr_speed);
                INFO("adjusted sync mtr speed by %d\n", sync_mtr_speed_adj);
            }
        } while (0);

        // sleep for 10 ms
        usleep(10000);  // 10 ms
        ms += 10;
    }

    // success
    return 0;
}


// XXX update this routine to drive fwd or rev,  or make 2 new routines
// XXX should this use secs?
int drive_rotate(double mph, double feet)
{
    return drive_common(mph, -mph, feet);
}
int drive_xxx(double lmph, double rmph, double feet)
{
    return drive_common(lmph, rmph, feet);
}
int drive_fwd(double mph, double feet)
{
    return drive_common(mph, mph, feet);
}
int drive_rew(double mph, double feet)
{
    return drive_common(-mph, -mph, feet);
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
        emer_stop_thread_state = EMER_STOP_THREAD_DISABLED;

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
            emer_stop_thread_state = EMER_STOP_THREAD_DISABLED; \
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
            int expected_speed = (8600./3200.) * mcs->target_speed[id];  // xxx  defines, or drive cal
            if ((expected_speed < 0 && actual_speed > expected_speed / 2) ||
                (expected_speed > 0 && actual_speed < expected_speed / 2))
            {
                if (enc_low_speed_start_time[id] == 0) {
                    enc_low_speed_start_time[id] = microsec_timer();
                }
            } else {
                enc_low_speed_start_time[id] = 0;
            }
            if (enc_low_speed_start_time[id] != 0 &&
                microsec_timer() - enc_low_speed_start_time[id] > 1000000)   // 1 sec
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
