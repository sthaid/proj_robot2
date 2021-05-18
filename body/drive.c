// XXX mag cal routine
// XXX drive_turn

#include "common.h"

//
// defines
//

#define EMER_STOP_THREAD_DISABLED  0
#define EMER_STOP_THREAD_ENABLED   1
#define EMER_STOP_OCCURRED         (emer_stop_thread_state == EMER_STOP_THREAD_DISABLED)

#define STOP_MOTORS_PRINT_NONE     0
#define STOP_MOTORS_PRINT_DISTANCE 1
#define STOP_MOTORS_PRINT_ROTATION 2

#define MC_ACCEL 5

//
// variables
//

static struct msg_drive_proc_s * drive_proc_msg;
static int                       emer_stop_thread_state;

//
// prototypes
//

static int stop_motors(int print);
static int mtr_speed_compensation(bool init, double rotation_target);

static void *drive_thread(void *cx);
static void *emer_stop_thread(void *cx);
static void emer_stop_button_cb(int id, bool pressed, int pressed_duration_us);

// -----------------  API  --------------------------------------------------

int drive_init(void)
{
    pthread_t tid;

    // register for left-button press, this will trigger an emergency stop
    button_register_cb(0, emer_stop_button_cb);

    // set mtr ctlr accel/decel limits, and debug mode
    mc_set_accel(MC_ACCEL, MC_ACCEL);
    mc_debug_mode(true);  // XXX temp

    // read the drive.cal file, and print
    if (drive_cal_file_read() < 0) {
        ERROR("drive_cal_file_read failed\n");
        return -1;
    }
    drive_cal_tbl_print();

    // create drive_thread and emer_stop_thread
    pthread_create(&tid, NULL, emer_stop_thread, NULL);
    pthread_create(&tid, NULL, drive_thread, NULL);
    
    // success
    return 0;
}

void drive_run(struct msg_drive_proc_s *drive_proc_msg_arg)
{
    static struct msg_drive_proc_s static_drive_proc_msg;

    if (drive_proc_msg != NULL) {
        ERROR("drive_proc is currently running\n");
        return;
    }

    static_drive_proc_msg = *drive_proc_msg_arg;
    __sync_synchronize();
    drive_proc_msg = &static_drive_proc_msg;
}

void drive_emer_stop(void)
{
    ERROR("emergency stop\n");
    mc_disable_all();
}

bool drive_emer_stop_occurred(void)
{
    return EMER_STOP_OCCURRED;
}

// -----------------  DRIVE PROCS SUPPORT ROUTINES  -------------------------

int drive_fwd(double feet, double mph)
{
    INFO("feet = %0.1f  mph = %0.1f\n", feet, mph);
    return drive_straight(feet, mph, NULL, NULL);
}

int drive_rev(double feet, double mph)
{
    INFO("feet = %0.1f  mph = %0.1f\n", feet, mph);
    return drive_straight(feet, -mph, NULL, NULL);
}

int drive_rotate(double desired_degrees, double fudge)
{
    double left_mtr_start_mph  = (desired_degrees > 0 ? 0.5 : -0.5);
    double right_mtr_start_mph = -left_mtr_start_mph;
    double left_mtr_mph        = (desired_degrees > 0 ? 0.3 : -0.3);
    double right_mtr_mph       = -left_mtr_mph;
    int    lspeed, rspeed;
    double start_degrees, rotated_degrees;

    INFO("desired_degrees = %0.1f  fudge = %0.1f\n", desired_degrees, fudge);

    // if caller has not supplied fudge factor then use builtin value
    if (fudge == 0) {
        static interp_point_t fudge_points[] = {
                { 15,   3.2  },
                { 30,   3.0  },
                { 90,   2.5  },
                { 180,  1.5  },
                { 270,  0.75 },
                { 360,  0.0  }, };
        fudge = interpolate(fudge_points, sizeof(fudge_points)/sizeof(interp_point_t), fabs(desired_degrees));
        INFO("builtin fudge = %0.2f\n", fudge);
    }

    // enable both front and read proximity sensors
    proximity_enable(0);   // enable front
    proximity_enable(1);   // enable rear

    // get imu rotation value prior to starting motors
    start_degrees = imu_get_rotation();

    // start motors using boost speed, and delay 200 ms
    lspeed = MTR_MPH_TO_SPEED(left_mtr_start_mph);
    rspeed = MTR_MPH_TO_SPEED(right_mtr_start_mph);
    if (mc_set_speed_all(lspeed, rspeed) < 0) {
        ERROR("failed to set mtr speeds to %d %d\n", lspeed, rspeed);
        return -1;
    }
    usleep(200000);  // 200 ms

    // slow down motors
    lspeed = MTR_MPH_TO_SPEED(left_mtr_mph);
    rspeed = MTR_MPH_TO_SPEED(right_mtr_mph);
    if (mc_set_speed_all(lspeed, rspeed) < 0) {
        ERROR("failed to set mtr speeds to %d %d\n", lspeed, rspeed);
        return -1;
    }

    // wait for rotation to complete
    int ms = 0;
    while (true) {
        // check if the emer_stop_thread shut down the motors
        if (EMER_STOP_OCCURRED) {
            ERROR("EMER_STOP_OCCURRED\n");
            return -1;
        }

        // check if rotation completed
        rotated_degrees = imu_get_rotation() - start_degrees;
        if ((ms % 1000) == 0) {
            INFO("rotated %0.1f deg\n", rotated_degrees);
        }
        if (fabs(rotated_degrees) > (fabs(desired_degrees) - fudge)) {
            break;
        }

        // sleep for 1 ms
        usleep(1000);
        ms += 1;
    }

    // stop motors
    if (stop_motors(STOP_MOTORS_PRINT_ROTATION) < 0) {
        return -1;
    }

    // print result
    rotated_degrees = imu_get_rotation() - start_degrees;
    INFO("done: desired = %0.1f  actual = %0.1f  deviation = %0.1f\n",
         desired_degrees, rotated_degrees, rotated_degrees - desired_degrees);

    // success
    return 0;
}

int drive_rotate_to_heading(double desired_heading, double fudge, bool disable_sdr)
{
    double current_heading, delta;
    double left_mtr_mph, right_mtr_mph;
    double left_mtr_start_mph, right_mtr_start_mph;
    int    ms, lspeed, rspeed;
    bool   clockwise;

    // if caller has not supplied fudge factor then use builtin value
    if (fudge == 0) {
        fudge = 8;
    }

    // sanitize the desired_heading into range 0 - 359.99
    desired_heading = sanitize_heading(desired_heading, 0);

    // determine the delta rotation to reach the desired heading
    current_heading = imu_get_magnetometer();
    delta = sanitize_heading(desired_heading - current_heading, -180);
    INFO("desired_heading = %0.1f  current_heading = %0.1f  delta = %0.1f  fudge = %0.1f\n", 
         desired_heading, current_heading, delta, fudge);

    // return immedeately if current heading is already very close to desired
    if (fabs(delta) < 2) {
        INFO("immedeate return due to small delta %0.1f\n", delta);
        return 0;
    }

    // enable both front and read proximity sensors
    proximity_enable(0);   // front
    proximity_enable(1);   // rear

    // if the delta rotation needed is small then rotate away
    // from desired_heading to provide a larger delta, to improve accuracy
    if (fabs(delta) < 30 && disable_sdr == false) {
        double tmp_hdg = sanitize_heading( (delta > 0 ? desired_heading - 30 : desired_heading + 30), 0 );
        INFO("small delta %0.1f, rotating from current %0.1f to tmp_hdg %0.1f\n",
             delta, current_heading, tmp_hdg);
        if (drive_rotate_to_heading(tmp_hdg, 0, true) < 0) {
            ERROR("small delta rotate failed\n");
            return -1;
        }
    }

    // determine the left and right mtr mph
    clockwise = (delta > 0);
    left_mtr_start_mph  = (clockwise ? 0.5 : -0.5);
    right_mtr_start_mph = -left_mtr_start_mph;
    left_mtr_mph        = (clockwise ? 0.3 : -0.3);
    right_mtr_mph       = -left_mtr_mph;

    // start motors using boost speed, and delay 200 ms
    lspeed = MTR_MPH_TO_SPEED(left_mtr_start_mph);
    rspeed = MTR_MPH_TO_SPEED(right_mtr_start_mph);
    if (mc_set_speed_all(lspeed, rspeed) < 0) {
        ERROR("failed to set mtr speeds to %d %d\n", lspeed, rspeed);
        return -1;
    }
    usleep(200000);  // 200 ms

    // slow down motors
    lspeed = MTR_MPH_TO_SPEED(left_mtr_mph);
    rspeed = MTR_MPH_TO_SPEED(right_mtr_mph);
    if (mc_set_speed_all(lspeed, rspeed) < 0) {
        ERROR("failed to set mtr speeds to %d %d\n", lspeed, rspeed);
        return -1;
    }

    // wait for rotation to complete
    ms = 0;
    while (true) {
        // check if the emer_stop_thread shut down the motors
        if (EMER_STOP_OCCURRED) {
            ERROR("EMER_STOP_OCCURRED\n");
            return -1;
        }

        // check if rotation to heading has completed
        current_heading = imu_get_magnetometer();
        delta = sanitize_heading(desired_heading - current_heading, -180);
        if ((ms % 1000) == 0) {
            INFO("current %0.1f deg   delta %0.1f\n", current_heading, delta);
        }
        if (clockwise) {
            if (delta < fudge) break;
        } else {
            if (delta > -fudge) break;
        }

        // sleep for 1 ms
        usleep(1000);
        ms += 1;
    }

    // stop motors
    if (stop_motors(STOP_MOTORS_PRINT_ROTATION) < 0) {
        return -1;
    }

    // print result
    current_heading = imu_get_magnetometer();
    delta = sanitize_heading(current_heading - desired_heading, -180);
    INFO("done: desired = %0.1f  current = %0.1f  deviation = %0.1f\n",
         desired_heading, current_heading, delta);

    // success
    return 0;
}

int drive_straight(double desired_feet, double mph, int *avg_lspeed_arg, int *avg_rspeed_arg)
{
    int lspeed, rspeed;
    double actual_feet, initial_degrees, rot_degrees;
    int initial_left_enc_count, initial_right_enc_count;
    int avg_lspeed_sum=0, avg_rspeed_sum=0, avg_speed_cnt=0, avg_lspeed, avg_rspeed;

    #define BOOST_MPH  0.5

    INFO("desired_feet = %0.1f  mph = %0.1f\n", desired_feet, mph);

    // preset avg mtr speed return values
    if (avg_lspeed_arg) *avg_lspeed_arg = 0;
    if (avg_rspeed_arg) *avg_rspeed_arg = 0;

    // if mph is zero then return
    if (mph == 0) {
        INFO("return because mph equals 0\n");
        return 0;
    }

    // get initial rotation and enc counts
    initial_degrees         = imu_get_rotation();
    initial_left_enc_count  = encoder_get_count(0);
    initial_right_enc_count = encoder_get_count(1);

    // if moving fwd then enable front prox sensor, else enable rear
    if (mph > 0) {
        proximity_enable(0);    // enable front
        proximity_disable(1);   // disable rear
    } else {
        proximity_disable(0);   // disable front
        proximity_enable(1);    // enable rear
    }

    // if mph is too low for starting then set mtr speeds for BOOST_MPH mph for 200 ms
    if (fabs(mph) < BOOST_MPH) {
        drive_cal_cvt_mph_to_mtr_speeds(mph > 0 ? BOOST_MPH : -BOOST_MPH, &lspeed, &rspeed);
        INFO("boot start - setting mtr speeds = %d %d\n", lspeed, rspeed);
        if (mc_set_speed_all(lspeed, rspeed) < 0) {
            ERROR("failed to set boost mtr speeds to %d %d\n", lspeed, rspeed);
            return -1;
        }
        usleep(200000);  // 200 ms
    }

    // get left and right mtr speeds from mph, and
    // set mtr speeds
    drive_cal_cvt_mph_to_mtr_speeds(mph, &lspeed, &rspeed);
    INFO("setting mtr speeds = %d %d\n", lspeed, rspeed);
    if (mc_set_speed_all(lspeed, rspeed) < 0) {
        ERROR("failed to set boost mtr speeds to %d %d\n", lspeed, rspeed);
        return -1;
    }

    // wait for distance_travelled to meet requested distance
    int ms = 0;
    while (true) {
        // check if the emer_stop_thread shut down the motors
        if (EMER_STOP_OCCURRED) {
            ERROR("EMER_STOP_OCCURRED\n");
            return -1;
        }

        // check for distance travelled completed
        actual_feet = ( ENC_COUNT_TO_FEET(encoder_get_count(0) - initial_left_enc_count) + 
                        ENC_COUNT_TO_FEET(encoder_get_count(1) - initial_right_enc_count) ) / 2;
        if (fabs(actual_feet) >= desired_feet) {
            break;
        }

        // perform motor speed compensation, and
        // maintain sums for calculation of avg mtr speeds
        if ((ms % 100) == 0) {
            int adj = mtr_speed_compensation(ms==0, initial_degrees);
            if (adj) {
                lspeed += adj;
                //INFO("*** adjusting left_mtr_speed by %d  new=%d\n", adj, lspeed);
                mc_set_speed(0, lspeed);
            }

            avg_lspeed_sum += lspeed;
            avg_rspeed_sum += rspeed;
            avg_speed_cnt++;
        }

        // sleep for 1 ms
        usleep(1000);  // 1 ms
        ms += 1;
    }

    // stop motors
    if (stop_motors(STOP_MOTORS_PRINT_DISTANCE) < 0) {
        return -1;
    }

    // calculate the avg speeds, and return the values if caller desires
    avg_lspeed = (avg_speed_cnt ? avg_lspeed_sum / avg_speed_cnt : 0);
    avg_rspeed = (avg_speed_cnt ? avg_rspeed_sum / avg_speed_cnt : 0);
    if (avg_lspeed_arg) *avg_lspeed_arg = avg_lspeed;
    if (avg_rspeed_arg) *avg_rspeed_arg = avg_rspeed;

    // print results
    actual_feet = ( ENC_COUNT_TO_FEET(encoder_get_count(0) - initial_left_enc_count) + 
                    ENC_COUNT_TO_FEET(encoder_get_count(1) - initial_right_enc_count) ) / 2;
    rot_degrees = imu_get_rotation() - initial_degrees;
    INFO("done: desired_feet = %0.1f  actual_feet = %0.1f  delta_feet = %0.1f  rot = %0.1f\n",
         desired_feet, actual_feet, desired_feet-actual_feet, rot_degrees);
    INFO("      average mtr speeds = %d %d\n", avg_lspeed, avg_rspeed);
    INFO("      actual  mtr speeds = %d %d\n", lspeed, rspeed);
    INFO("      delta   mtr speeds = %d %d\n", avg_lspeed-lspeed, avg_rspeed-rspeed);

    // success
    return 0;
}

// XXX needs work
int drive_mag_cal(void)
{
    // enable magnetometer calibration
    imu_set_mag_cal_ctrl(MAG_CAL_CTRL_ENABLED);

    // rotate 10 times
//xxx check rc
    drive_rotate(10 * 360, 0);

    // disable magnetometer calibration, and save calibration file
    imu_set_mag_cal_ctrl(MAG_CAL_CTRL_DISABLED_SAVE);

    return 0;
}

static int stop_motors(int print)
{
    uint64_t start_us;
    int      left_enc_count, right_enc_count;
    double   rotation;

    // get encoder counts, and rotation  prior to stopping, these will
    // be used to determine the stopping distance and rotation below
    left_enc_count = encoder_get_count(0);
    right_enc_count = encoder_get_count(1);
    rotation = imu_get_rotation();

    // set all motor speeds to 0
    if (mc_set_speed_all(0,0) < 0) {
        ERROR("failed to set mtr speeds to 0 0\n");
        return -1;
    }

    // wait up to 1.5 second for encoder speed to drop to 0
    start_us = microsec_timer();
    while (true) {
        if (EMER_STOP_OCCURRED) {
            ERROR("EMER_STOP_OCCURRED\n");
            return -1;
        }
        if (microsec_timer()-start_us > 1500000) {  // 1.5 sec
            ERROR("failed to stop within 1.5 seconds\n");
            return -1;
        }
        if (encoder_get_speed(0) == 0 && encoder_get_speed(1) == 0) {
            break;
        }
        usleep(1000);  // 1 ms
    }

    // print the amount of distance or rotation during the stop motors
    if (print == STOP_MOTORS_PRINT_DISTANCE) {
        double stopping_distance = 
            ( fabs(ENC_COUNT_TO_FEET(encoder_get_count(0) - left_enc_count)) + 
              fabs(ENC_COUNT_TO_FEET(encoder_get_count(1) - right_enc_count)) )
            / 2;
        INFO("done: stopping distance = %0.2f ft  duration = %0.2f s\n",
             stopping_distance, (microsec_timer() - start_us) / 1000000.);
    } else if (print == STOP_MOTORS_PRINT_ROTATION) {
        double stopping_rotation = imu_get_rotation() - rotation;
        INFO("done: stopping rotation = %0.2f degs  duration = %0.2f s\n",
             stopping_rotation, (microsec_timer() - start_us) / 1000000.);
    }

    // success
    return 0;
}

// returns adjustment value that can be applied either
// directly to the left mtr, or negated to the right mtr
static int mtr_speed_compensation(bool init, double rotation_target)
{
    #define PREDICTION_INTERVAL_SECS  0.5
    #define TUNE  3.0

    uint64_t        time_now;
    double          rotation_now, rotation_rate, rotation_predicted;
    double          duration_secs;
    int             compensation;

    static double   rotation_last;
    static uint64_t time_last;

    time_now = microsec_timer();

    if (init) {
        time_last = time_now;
        rotation_last = imu_get_rotation();
        return 0;
    }

    duration_secs      = (time_now - time_last) / 1000000.;
    rotation_now       = imu_get_rotation();
    rotation_rate      = (rotation_now - rotation_last) / duration_secs;
    rotation_predicted = rotation_now + rotation_rate * PREDICTION_INTERVAL_SECS;

    compensation = nearbyint( (rotation_target - rotation_predicted) * TUNE );

    //INFO("dur = %0.3f  rot_now = %0.3f  rot_rate = %0.3f rot_pred = %0.3f comp = %d\n",
    //     duration_secs, rotation_now, rotation_rate, rotation_predicted, compensation);

    if (compensation > 10) {
        compensation = 10;
    } else if (compensation < -10) {
        compensation = -10;
    }

    time_last = time_now;
    rotation_last = rotation_now;

    return compensation;
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
        while (drive_proc_msg == NULL) {
            usleep(10000);  // 10 ms
        }

        // reset encoders, and imu rotatation
        encoder_count_reset(0);
        encoder_count_reset(1);
        imu_reset_rotation();

        // enable motor-ctlr and sensors (except proximity is left disabled),
        // enable emer_stop_thread, and
        // wait for 15 ms to allow time for the enables to take affect
        if (mc_enable_all() < 0) {
            drive_proc_msg = NULL;
            continue;
        }
        encoder_enable(0);
        encoder_enable(1);
        proximity_disable(0);
        proximity_disable(1);
        imu_set_accel_rot_ctrl(true);
        emer_stop_thread_state = EMER_STOP_THREAD_ENABLED;
        usleep(15000);  // 15 ms

        // call the drive proc
        drive_proc(drive_proc_msg);

        // disable emer_stop_thread, and
        // disable motor-ctlr and sensors
        emer_stop_thread_state = EMER_STOP_THREAD_DISABLED;
        mc_disable_all();
        encoder_disable(0);
        encoder_disable(1);
        proximity_disable(0);
        proximity_disable(1);
        imu_set_accel_rot_ctrl(false);

        // clear drive_proc_msg
        drive_proc_msg = NULL;
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
            double enc_mph = ENC_SPEED_TO_MPH(encoder_get_speed(id));
            double mtr_mph = MTR_SPEED_TO_MPH(mcs->target_speed[id]);  // XXX or use drive cal
            if ((mtr_mph >= 0 && enc_mph < mtr_mph / 2) ||
                (mtr_mph <  0 && enc_mph > mtr_mph / 2))
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
                DO_EMER_STOP("enc=%d mtr_mph=%0.1f enc_mph=%0.1f\n", id, mtr_mph, enc_mph);
            }
        }

        usleep(1000);  // 1 ms
    }

    return NULL;
}

static void emer_stop_button_cb(int id, bool pressed, int pressed_duration_us)
{
    if (pressed) {
        stop_button_pressed = true;
    }
}
