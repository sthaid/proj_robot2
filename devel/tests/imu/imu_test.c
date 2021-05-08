#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <imu.h>

int main(int argc, char **argv)
{
    int count = 0;
    bool accel_alert;
    double accel_alert_value;

    if (imu_init(0)) {
        printf("imu_init failed\n");
        return 1;
    }

    // magnetometer calibration 
    if (argc == 2 && strcmp(argv[1], "mag_cal") == 0) {
        printf("mag cal test\n");
        sleep(10);

        printf("- start motion\a\n");
        imu_set_mag_cal_ctrl(MAG_CAL_CTRL_ENABLED);
        sleep(60);
        imu_set_mag_cal_ctrl(MAG_CAL_CTRL_DISABLED_SAVE);
        printf("- done\a\n");
        sleep(3);

        return 0;
    }

    // read mag heading, rotation, and acceleration
    imu_set_accel_rot_ctrl(true);
    while (1) {
        if ((++count % 100) == 0) {
            printf("heading  %6.0f   rotation %6.0f\n", 
                   imu_get_magnetometer(), imu_get_rotation());
        }

        accel_alert = imu_check_accel_alert(&accel_alert_value);
        if (accel_alert) {
            printf("\aALERT: %0.1f\n", accel_alert_value);
        }

        usleep(10000);
    }

    return 0;
}
