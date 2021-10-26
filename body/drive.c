// xxx review and comment out debug prints

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
static char                      emer_stop_reason[200];
static mc_status_t             * mcs;

//
// prototypes
//

static int drive_rotate_to_heading_ex(double desired_heading, double fudge, bool do_sdr_check);
static int stop_motors(int print);

static int drive_straight(double desired_feet, double mph, bool stop_motors_flag,
                          int *avg_lspeed_arg, int *avg_rspeed_arg);
static int drive_straight_mtr_speed_comp(bool init, double rotation_target);

static void drive_straight_cal_cvt_mph_to_mtr_speeds(double mph, int *lspeed, int *rspeed);
static int drive_straight_cal_file_read(void);
static int drive_straight_cal_file_write(void);
static void drive_straight_cal_tbl_print(void);
static int drive_straight_cal_proc(double cal_feet);
static void drive_straight_cal_tbl_init_default(void);

static void *drive_thread(void *cx);
static void *emer_stop_thread(void *cx);
static void emer_stop_button_cb(int id, bool pressed, int pressed_duration_us);

// -----------------  API  --------------------------------------------------

int drive_init(void)
{
    pthread_t tid;

    // get pointer to mtr ctlr status
    mcs = mc_get_status();

    // register for left-button press, this will trigger an emergency stop
    button_register_cb(0, emer_stop_button_cb);

    // set mtr ctlr accel/decel limits, and disable debug mode
    mc_set_accel(MC_ACCEL, MC_ACCEL);
    mc_debug_mode(false);

    // read the drive_straight.cal file, and print
    if (drive_straight_cal_file_read() < 0) {
        ERROR("drive_straight_cal_file_read failed\n");
        return -1;
    }
    drive_straight_cal_tbl_print();

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
        ERROR("drive is busy\n");
        send_drive_proc_complete_msg(drive_proc_msg_arg->unique_id, "drive is busy");
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

// -----------------  ROUTINES CALLED FROM DRIVE_PROCS.C  -------------------

int drive_straight_cal(double cal_feet)
{
    return drive_straight_cal_proc(cal_feet);
}

int drive_mag_cal(double num_rot)
{
    // enable magnetometer calibration
    imu_set_mag_cal_ctrl(MAG_CAL_CTRL_ENABLED);

    // rotate 10 times
    if (drive_rotate(num_rot * 360, 0) < 0) {
        ERROR("drive_rotate for %0.0f revs failed\n", num_rot);
        return -1;
    }

    // disable magnetometer calibration, and save calibration file
    imu_set_mag_cal_ctrl(MAG_CAL_CTRL_DISABLED_SAVE);

    return 0;
}

int drive_fwd(double feet, double mph, bool stop_motors_flag)
{
    INFO("feet = %0.1f  mph = %0.1f\n", feet, mph);
    return drive_straight(feet, mph, stop_motors_flag, NULL, NULL);
}

int drive_rev(double feet, double mph)
{
    INFO("feet = %0.1f  mph = %0.1f\n", feet, mph);
    return drive_straight(feet, -mph, true, NULL, NULL);
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

    // disable both front and read proximity sensors when rotating
    proximity_disable(0);   // disable front
    proximity_disable(1);   // disable rear

    // get imu rotation value prior to starting motors
    start_degrees = imu_get_rotation();

    // if either motor was left running then stop the motors
    if (mcs->target_speed[0] != 0 || mcs->target_speed[1] != 0) {
        if (stop_motors(STOP_MOTORS_PRINT_NONE) < 0) {
            return -1;
        }
    }

    // start motors using boost speed (0.5 mph), and delay 200 ms
    lspeed = MTR_MPH_TO_SPEED(left_mtr_start_mph);
    rspeed = MTR_MPH_TO_SPEED(right_mtr_start_mph);
    if (mc_set_speed_all(lspeed, rspeed) < 0) {
        ERROR("failed to set mtr speeds to %d %d\n", lspeed, rspeed);
        return -1;
    }
    usleep(200000);  // 200 ms

    // slow down motors to 0.3 mph
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

int drive_rotate_to_heading(double desired_heading, double fudge)
{
    return drive_rotate_to_heading_ex(desired_heading, fudge, true);
}

static int drive_rotate_to_heading_ex(double desired_heading, double fudge, bool do_sdr_check)
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

    // disable both front and read proximity sensors when rotating
    proximity_disable(0);   // disable front
    proximity_disable(1);   // disable rear

    // if the delta rotation needed is small then rotate away
    // from desired_heading to provide a larger delta, to improve accuracy
    // (do_sdr_check = do small delta rotation check)
    if (fabs(delta) < 30 && do_sdr_check == true) {
        double tmp_hdg = sanitize_heading( (delta > 0 ? desired_heading - 30 : desired_heading + 30), 0 );
        INFO("small delta %0.1f, rotating from current %0.1f to tmp_hdg %0.1f\n",
             delta, current_heading, tmp_hdg);
        if (drive_rotate_to_heading_ex(tmp_hdg, 0, false) < 0) {
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

    // if either motor was left running then stop the motors
    if (mcs->target_speed[0] != 0 || mcs->target_speed[1] != 0) {
        if (stop_motors(STOP_MOTORS_PRINT_NONE) < 0) {
            return -1;
        }
    }

    // start motors using boost speed (0.5 mph), and delay 200 ms
    lspeed = MTR_MPH_TO_SPEED(left_mtr_start_mph);
    rspeed = MTR_MPH_TO_SPEED(right_mtr_start_mph);
    if (mc_set_speed_all(lspeed, rspeed) < 0) {
        ERROR("failed to set mtr speeds to %d %d\n", lspeed, rspeed);
        return -1;
    }
    usleep(200000);  // 200 ms

    // slow down motors to 0.3 mph
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

int drive_radius(double desired_degrees, double radius_feet, bool stop_motors_flag, double fudge)
{
    double left_mtr_mph, right_mtr_mph, mtr_a_mph, mtr_b_mph, boost;
    int    lspeed, rspeed;
    double start_degrees, rotated_degrees;
    bool   boost_start_needed;

    #define WHEEL_BASE_FEET (10./12.)

    INFO("desired_degrees = %0.1f  radius_feet =  %0.1f  fudge = %0.1f\n", 
         desired_degrees, radius_feet, fudge);

    // validate radius arg
    if (radius_feet < 0.8 && radius_feet != 0) {
        ERROR("invalid radius %0.1f ft\n", radius_feet);
        return -1;
    }

    // return immedeately if desired_degrees is small
    if (fabs(desired_degrees) < 2) {
        INFO("immedeate return due to small desired_degrees %0.1f\n", desired_degrees);
        return 0;
    }

    // if caller has not supplied fudge factor then use builtin value
    if (fudge == 0) {
        if (stop_motors_flag) {
            static interp_point_t fudge_points_radius_0[] = {  // r = 0 ft
                    { 45,   5.0  },
                    { 90,   4.0  },
                    { 180,  3.0  },
                    { 360,  0.0  }, };
            static interp_point_t fudge_points_radius_1_0[] = {  // r = 1.0 ft
                    { 45,   4.0  },
                    { 90,   3.5  },
                    { 180,  3.0  },
                    { 360,  0.0  }, };
            static interp_point_t fudge_points_radius_1_5[] = {  // r = 1.5 ft
                    { 45,   2.5  },
                    { 90,   2.5  },
                    { 180,  3.0  },
                    { 360,  0.0  }, };
            interp_point_t * fudge_points;

            if (radius_feet == 0) {
                fudge_points = fudge_points_radius_0;
            } else if (radius_feet < 1.25) {
                fudge_points = fudge_points_radius_1_0;
            } else {
                fudge_points = fudge_points_radius_1_5;
            }
            fudge = interpolate(fudge_points, 4, fabs(desired_degrees));
            INFO("builtin fudge = %0.2f\n", fudge);
        } else { // stop_motors_flag == false
            fudge = -1;
        }
    }

    // determine speed of motors based on radius, with the following criteria:
    // - ratio of motor speeds chosen to achieve desired turn radius, where the
    //   radius is measured from the inside wheel
    // - speeds >= 0.3 mph
    // - if both have speeds < 0.5 then increase the speeds so that the 
    //   faster motor is 0.5
    // - mtr_a_speed > mtr_b_speed
    if (radius_feet == 0) {
        mtr_a_mph = 0.5;
        mtr_b_mph = 0;
    } else {
        double ratio = (WHEEL_BASE_FEET + radius_feet) / radius_feet;
        mtr_a_mph = 0.3 * ratio;
        mtr_b_mph = 0.3;
        if (mtr_a_mph < 0.5) {
            double x = 0.5 / mtr_a_mph;
            mtr_a_mph *= x;
            mtr_b_mph *= x;
        }
    }

    // a motor start boost is required if the slower of the 2 motors (mtr_b_mph) 
    // is less than 0.5 mph; the startup boost will ensure that both motors start
    // at >= 0.5 mph
    boost = 0;
    if (mtr_b_mph != 0 && mtr_b_mph < 0.5) {
        boost = 0.5 / mtr_b_mph;
    }

    // assign the left/right mtr mph using the 2 motor speeds determined above, 
    // and the direction of the rotation
    if (desired_degrees > 0) {
        left_mtr_mph  = mtr_a_mph;
        right_mtr_mph = mtr_b_mph;
    } else {
        left_mtr_mph  = mtr_b_mph;
        right_mtr_mph = mtr_a_mph;
    }

    // enable front proximity sensors
    proximity_enable(0);   // enable front
    proximity_disable(1);  // disable rear

    // get imu rotation value prior to starting motors
    start_degrees = imu_get_rotation();

    // if either motor was left running in reverse then
    // return error because the motors should never be left running in reverse
    if (mcs->target_speed[0] < 0 || mcs->target_speed[1] < 0) {
        ERROR("motors should never be left running in reverse, %d %d\n",
            mcs->target_speed[0], mcs->target_speed[1]);
        return -1;
    }

    // boost_start_needed =
    //   boost != 0 &&
    //   ((left_mtr_mph != 0 && left mtr is currently not running) ||
    //    (right_mtr_mph != 0 && right mtr is currently not running))
    // 
    // if boost_start_needed then 
    //   start motors using boost speed,
    //   delay 200 ms
    // endif
    boost_start_needed = (boost != 0) &&
                         ((left_mtr_mph != 0 && mcs->target_speed[0] == 0) ||
                          (right_mtr_mph != 0 && mcs->target_speed[1] == 0));
    if (boost_start_needed) {
        INFO("**** BOOST START %0.2f - MPH %0.2f %0.2f\n", boost, boost*left_mtr_mph, boost*right_mtr_mph);
        lspeed = MTR_MPH_TO_SPEED(boost * left_mtr_mph);
        rspeed = MTR_MPH_TO_SPEED(boost * right_mtr_mph);
        if (mc_set_speed_all(lspeed, rspeed) < 0) {
            ERROR("failed to set mtr speeds to %d %d\n", lspeed, rspeed);
            return -1;
        }
        usleep(200000);  // 200 ms
    }

    // set motors to desired speed
    INFO("**** NORMAL MPH  %0.2f %0.2f\n", left_mtr_mph, right_mtr_mph);
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
    if (stop_motors_flag) {
        if (stop_motors(STOP_MOTORS_PRINT_ROTATION) < 0) {
            return -1;
        }
    }

    // print result
    rotated_degrees = imu_get_rotation() - start_degrees;
    INFO("done: desired = %0.1f  actual = %0.1f  deviation = %0.1f\n",
         desired_degrees, rotated_degrees, rotated_degrees - desired_degrees);

    // success
    return 0;
}

// - - - - - - - - - - - - - 

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

// -----------------  DRIVE STRAIGHT SUPPORT  -------------------------------

static int drive_straight(double desired_feet, double mph, bool stop_motors_flag,
                          int *avg_lspeed_arg, int *avg_rspeed_arg)
{
    int    lspeed, rspeed;
    double actual_feet, initial_degrees, rot_degrees;
    int    initial_left_enc_count, initial_right_enc_count;
    int    avg_lspeed_sum=0, avg_rspeed_sum=0, avg_speed_cnt=0, avg_lspeed, avg_rspeed;
    bool   boost_start_needed;

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

    // if either motor was left running in reverse then
    // return error because the motors should never be left running in reverse
    if (mcs->target_speed[0] < 0 || mcs->target_speed[1] < 0) {
        ERROR("motors should never be left running in reverse, %d %d\n",
            mcs->target_speed[0], mcs->target_speed[1]);
        return -1;
    }

    // if request is to drive reverse and either motor was left running 
    // then stop the motors
    if ((mph < 0) && (mcs->target_speed[0] != 0 || mcs->target_speed[1] != 0)) {
        if (stop_motors(STOP_MOTORS_PRINT_NONE) < 0) {
            return -1;
        }
    }

    // boost_start_needed =
    //  fabs(mph) < 0.5 &&
    //  (left mtr is currently not running || right mtr is currently not running)
    // 
    // if boost_start_needed then 
    //   start motors using boost speed,
    //   delay 200 ms
    // endif
    boost_start_needed = (fabs(mph) < 0.5) &&
                         (mcs->target_speed[0] == 0 || mcs->target_speed[1] == 0);
    if (boost_start_needed) {
        drive_straight_cal_cvt_mph_to_mtr_speeds(mph > 0 ? 0.5 : -0.5, &lspeed, &rspeed);
        INFO("boot start - setting mtr speeds = %d %d\n", lspeed, rspeed);
        if (mc_set_speed_all(lspeed, rspeed) < 0) {
            ERROR("failed to set boost mtr speeds to %d %d\n", lspeed, rspeed);
            return -1;
        }
        usleep(200000);  // 200 ms
    }

    // get left and right mtr speeds from mph, and
    // set mtr speeds
    drive_straight_cal_cvt_mph_to_mtr_speeds(mph, &lspeed, &rspeed);
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
            int adj = drive_straight_mtr_speed_comp(ms==0, initial_degrees);
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
    if (stop_motors_flag) {
        if (stop_motors(STOP_MOTORS_PRINT_DISTANCE) < 0) {
            return -1;
        }
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

// returns adjustment value that can be applied either
// directly to the left mtr, or negated to the right mtr
static int drive_straight_mtr_speed_comp(bool init, double rotation_target)
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

// - - - - - - - - -  DRIVE STRAIGHT SUPPORT - CALIBRATION   - - - - - -

#define MAX_DRIVE_STRAIGHT_CAL_TBL   30
#define DRIVE_STRAIGHT_CAL_FILENAME  "drive_straight.cal"

static struct drive_straight_cal_s {
    double mph;
    int    lspeed;
    int    rspeed;
} drive_straight_cal_tbl[MAX_DRIVE_STRAIGHT_CAL_TBL];

static void drive_straight_cal_cvt_mph_to_mtr_speeds(double mph, int *lspeed, int *rspeed)
{
    int idx, min_idx=-1;
    double min_delta = 1000000;
    double delta;

    // loop over the drive_straight_cal_tbl and find the entry with mph nearest
    // what caller is requesting
    for (idx = 0; idx < MAX_DRIVE_STRAIGHT_CAL_TBL; idx++) {
        struct drive_straight_cal_s *x = &drive_straight_cal_tbl[idx];
        if (x->mph == 0) {
            break;
        }
        delta = fabs(mph - x->mph);
        if (delta < min_delta) {
            min_delta = delta;
            min_idx = idx;
        }
    }

    // return the lspeed and rspeed
    if (idx == -1) {
        FATAL("bug\n");
    }
    *lspeed = drive_straight_cal_tbl[min_idx].lspeed;
    *rspeed = drive_straight_cal_tbl[min_idx].rspeed;

    // debug print result
    INFO("requested mph = %0.1f  nearest_mph = %0.1f  l/rspeed = %d %d\n",
         mph, drive_straight_cal_tbl[min_idx].mph, *lspeed, *rspeed);
}

static int drive_straight_cal_file_read(void)
{
    FILE *fp;
    int idx=0;
    char s[100];

    fp = fopen(DRIVE_STRAIGHT_CAL_FILENAME, "r");
    if (fp == NULL) {
        WARN("failed to open %s, using default drive_straight_cal_tbl, %s\n", 
             DRIVE_STRAIGHT_CAL_FILENAME, strerror(errno));
        drive_straight_cal_tbl_init_default();
        return 0;
    }
    while (fgets(s, sizeof(s), fp)) {
        if (idx == MAX_DRIVE_STRAIGHT_CAL_TBL) {
            ERROR("too man entries in %s\n", DRIVE_STRAIGHT_CAL_FILENAME);
            return -1;
        }

        struct drive_straight_cal_s *x = &drive_straight_cal_tbl[idx];
        if (sscanf(s, "%lf %d %d", &x->mph, &x->lspeed, &x->rspeed)!= 3) {
            ERROR("invalid line: %s\n", s);
            return -1;
        }
        idx++;
    }
    fclose(fp);

    return 0;
}

static int drive_straight_cal_file_write(void)
{
    FILE *fp;
    int idx;

    fp = fopen(DRIVE_STRAIGHT_CAL_FILENAME, "w");
    if (fp == NULL) {
        ERROR("failed to open %s for writing, %s\n", 
              DRIVE_STRAIGHT_CAL_FILENAME, strerror(errno));
        return -1;
    }
    for (idx = 0; idx < MAX_DRIVE_STRAIGHT_CAL_TBL; idx++) {
        struct drive_straight_cal_s *x = &drive_straight_cal_tbl[idx];
        if (x->mph == 0) {
            break;
        }
        fprintf(fp, "%4.1f %5d %5d\n", x->mph, x->lspeed, x->rspeed);
    }
    fclose(fp);

    return 0;
}

static void drive_straight_cal_tbl_print(void)
{
    int idx;

    INFO(" MPH LSPEED RSPEED\n");
    INFO(" --- ------ ------\n");
    for (idx = 0; idx < MAX_DRIVE_STRAIGHT_CAL_TBL; idx++) {
        struct drive_straight_cal_s *x = &drive_straight_cal_tbl[idx];
        if (x->mph == 0) {
            break;
        }
        INFO("%4.1f %6d %6d\n", x->mph, x->lspeed, x->rspeed);
    }
}

static int drive_straight_cal_proc(double cal_feet)
{
    int idx, lspeed, rspeed;
    struct drive_straight_cal_s new_drive_straight_cal_tbl[MAX_DRIVE_STRAIGHT_CAL_TBL];

    // make copy of drive_straight_cal_tbl
    memcpy(new_drive_straight_cal_tbl, drive_straight_cal_tbl, sizeof(drive_straight_cal_tbl));

    // loop over the new_drive_straight_cal_tbl, 
    // and update the lspeed and rspeed values
    for (idx = 0; idx < MAX_DRIVE_STRAIGHT_CAL_TBL; idx++) {
        struct drive_straight_cal_s *new = &new_drive_straight_cal_tbl[idx];
        if (new->mph == 0) {
            break;
        }

        INFO("calibrate mph %0.1f\n", new->mph);
        if (drive_straight(cal_feet, new->mph, true, &lspeed, &rspeed) < 0) {
            ERROR("drive_straight failed\n");
            break;
        }
        new->lspeed = lspeed;
        new->rspeed = rspeed;
    }

    // print the current and new drive_straight_cal_tbl values
    INFO("          CURRENT           NEW            DELTA\n");
    INFO(" MPH   LSPEED RSPEED   LSPEED RSPEED   LSPEED RSPEED\n");
    INFO(" ---   ------ ------   ------ ------   ------ ------\n");
    for (idx = 0; idx < MAX_DRIVE_STRAIGHT_CAL_TBL; idx++) {
        struct drive_straight_cal_s *cur = &drive_straight_cal_tbl[idx];
        struct drive_straight_cal_s *new = &new_drive_straight_cal_tbl[idx];
        if (new->mph == 0) {
            break;
        }
        INFO("%4.1f   %6d %6d   %6d %6d   %6d %6d\n", 
             cur->mph, 
             cur->lspeed, cur->rspeed,
             new->lspeed, new->rspeed,
             new->lspeed-cur->lspeed, new->rspeed-cur->rspeed);
    }

    // publish the new_drive_straight_cal_tbl to drive_straight_cal_tbl,
    // write drive_straight_cal_tbl to file
    memcpy(drive_straight_cal_tbl, new_drive_straight_cal_tbl, sizeof(drive_straight_cal_tbl));
    drive_straight_cal_file_write();
    return 0;
}

static void drive_straight_cal_tbl_init_default(void)
{
    int n, idx = 0;
    struct drive_straight_cal_s *x;

    // init drive_straight_cal_tbl to default, in case the drive cal file doesn't exist
    for (n = 3; n <= 8; n++) {
        double mph = 0.1 * n;

        // add entries for mph
        x = &drive_straight_cal_tbl[idx];
        x->mph = mph;
        x->lspeed = x->rspeed = MTR_MPH_TO_SPEED(x->mph);
        idx++;

        // add entries for -mph
        x = &drive_straight_cal_tbl[idx];
        x->mph = -mph;
        x->lspeed = x->rspeed = MTR_MPH_TO_SPEED(x->mph);
        idx++;
    }
}

// -----------------  THREADS  ----------------------------------------------

// - - - - - - - - -  DRIVE_THREAD   - - - - - - - - - - - - - - - - - 

static void *drive_thread(void *cx)
{
    struct sched_param param;
    int rc, unique_id;

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

        // make copy of unique_id, and clear emer_stop_reason
        unique_id = drive_proc_msg->unique_id;
        emer_stop_reason[0] = '\0';

        // reset encoders, and imu rotatation
        encoder_count_reset(0);
        encoder_count_reset(1);
        imu_reset_rotation();

        // enable motor-ctlr and sensors (except proximity is left disabled),
        // enable emer_stop_thread, and
        // wait for 15 ms to allow time for the enables to take affect
        if (mc_enable_all() < 0) {
            strcpy(emer_stop_reason, "failed to enable motors");
            rc = -1;
            goto done;
        }
        encoder_enable(0);
        encoder_enable(1);
        proximity_disable(0);
        proximity_disable(1);
        imu_set_accel_rot_ctrl(true);
        emer_stop_thread_state = EMER_STOP_THREAD_ENABLED;
        usleep(15000);  // 15 ms

        // call the drive proc
        rc = drive_proc(drive_proc_msg);

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
done:   drive_proc_msg = NULL;

        // send drive proc complete msg
        if (rc == -1 && emer_stop_reason[0] == '\0') {
            strcpy(emer_stop_reason, "unknown error");
        }
        send_drive_proc_complete_msg(unique_id, emer_stop_reason);
    }

    return NULL;
}

// - - - - - - - - -  EMER_STOP_THREAD - - - - - - - - - - - - - - - - 

static uint64_t enc_low_speed_start_time[2];
static bool     stop_button_pressed;

static void *emer_stop_thread(void *cx)
{
    int encoder_errs;
    double accel;
    struct sched_param param;
    int rc;

    #define DO_EMER_STOP(fmt, args...) \
        do { \
            sprintf(emer_stop_reason, fmt, ## args); \
            ERROR("%s\n", emer_stop_reason); \
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
            DO_EMER_STOP("motors have been disabled\n");
        }

        if (stop_button_pressed) {
            DO_EMER_STOP("stop button\n");
        }

        if ((encoder_errs = encoder_get_errors(0))) {
            DO_EMER_STOP("left wheel encoder has %d errors", encoder_errs);
        }
        if ((encoder_errs = encoder_get_errors(1))) {
            DO_EMER_STOP("right wheel encoder has %d errors", encoder_errs);
        }

        if (proximity_check(0,NULL)) {
            DO_EMER_STOP("front proximity alert");
        }
        if (proximity_check(1,NULL)) {
            DO_EMER_STOP("rear proximity alert");
        }

        if (imu_check_accel_alert(&accel)) {
            DO_EMER_STOP("acceleration %0.1f g\n", accel);
        }

        for (int id = 0; id < 2; id++) {
            double enc_mph = ENC_SPEED_TO_MPH(encoder_get_speed(id));
            double mtr_mph = MTR_SPEED_TO_MPH(mcs->target_speed[id]);  // xxx or use drive cal
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
                DO_EMER_STOP("%s encoder speed %0.1f does not match motor speed %0.1f",
                             id == 0 ? "left" : "right", enc_mph, mtr_mph);
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
