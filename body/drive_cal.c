#include "common.h"

//
// defines
//

//
// variables
//

static struct {
    int    speed;
    double left_mph;
    double right_mph;
} drive_cal_tbl[32];

// -----------------  DRIVE CALIBRATION  -------------------------------------

int drive_cal_file_read(void)
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

int drive_cal_file_write(void)
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

void drive_cal_tbl_print(void)
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

int drive_cal_proc(void)
{
    int speed_tbl[] = {-MAX_MTR_SPEED, -MIN_MTR_SPEED, MIN_MTR_SPEED, MAX_MTR_SPEED};

    #define MAX_SPEED_TBL (sizeof(speed_tbl)/sizeof(speed_tbl[0]))

    #define CAL_INTVL_US  (15 * 1000000)

    INFO("starting %s\n", __func__);

    for (int i = 0; i < MAX_SPEED_TBL; i++) {
        uint64_t start_us, duration_us;
        int speed = speed_tbl[i];
        int left_enc, right_enc;

        // set both motors to speed, and wait to stabilize
        INFO("calibrating motor speed %d\n", speed);
        mc_set_speed_all(speed,speed);
        if (drive_sleep(2000000) < 0) {   // 2 secs
            return -1;
        }

        // reset encoder positions
        // sleep for CAL_INTVL_US
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
        drive_cal_tbl[i].left_mph  = (( left_enc/2248.86) * (.080*M_PI) / (duration_us/1000000.)) * 2.23694;
        drive_cal_tbl[i].right_mph = ((right_enc/2248.86) * (.080*M_PI) / (duration_us/1000000.)) * 2.23694;
    }

    // debug print the cal_tbl
    drive_cal_tbl_print();

    // write cal_tbl to file
    drive_cal_file_write();

    // success
    return 0;
}

int drive_cal_cvt_mph_to_left_motor_speed(double mph, int *left_mtr_speed)
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

int drive_cal_cvt_mph_to_right_motor_speed(double mph, int *right_mtr_speed)
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
