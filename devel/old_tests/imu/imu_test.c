#include <stdio.h>
#include <unistd.h>

#include <body.h>
#include <imu.h>

int main(int argc, char **argv)
{
    double heading;

    if (imu_init(0)) {
        printf("imu_init failed\n");
        return 1;
    }

    while (1) {
        sleep(1);
        imu_read_magnetometer(&heading);
        printf("heading %0.0f\n", heading);
    }

    return 0;
}
