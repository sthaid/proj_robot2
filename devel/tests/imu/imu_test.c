#include <stdio.h>
#include <unistd.h>

#include <body.h>
#include <imu.h>

int main(int argc, char **argv)
{
    int count = 0;
    double heading;
    bool accel_alert;
    double accel_alert_value;

    if (imu_init(0)) {
        printf("imu_init failed\n");
        return 1;
    }

    while (1) {
        if ((++count % 1000) == 0) {
            imu_read_magnetometer(&heading);
            printf("heading %0.0f\n", heading);
        }

        imu_check_accel_alert(&accel_alert, &accel_alert_value);
        if (accel_alert) {
            printf("\aALERT: %0.1f\n", accel_alert_value);
        }

        usleep(1000);
    }

    return 0;
}
