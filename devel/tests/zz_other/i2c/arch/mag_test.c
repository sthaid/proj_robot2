#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <math.h>

#include <misc.h>
#include <MPU9250_imu.h>

int main(int argc, char **argv)
{
    int16_t mx, my, mz;
    int     mx_min=+1000000, my_min=+1000000, mz_min=+1000000;
    int     mx_max=-1000000, my_max=-1000000, mz_max=-1000000;
    int     mx_ctr, my_ctr, mz_ctr;
    double  heading;
    time_t  time_done;
    bool    perform_cal;

    perform_cal = (argc > 1) && (strcmp(argv[1], "cal") == 0);
    
    if (MPU9250_imu_init(0) < 0) {
        ERROR("MPU9250_imu_init failed\n");
        return 1;
    }

    // calibration 
    if (perform_cal) {
        printf("CALIBRATION STARTING\a\n");

        time_done = time(NULL) + 120;
        while (time(NULL) < time_done) {
            MPU9250_imu_get_magnetometer(&mx, &my, &mz);
            //printf(" - cal read  %d %d %d\n", mx, my, mz);
            usleep(20000);

            if (mx < mx_min) mx_min = mx;
            if (my < my_min) my_min = my;
            if (mz < mz_min) mz_min = mz;

            if (mx > mx_max) mx_max = mx;
            if (my > my_max) my_max = my;
            if (mz > mz_max) mz_max = mz;
        }

        mx_ctr = (mx_max + mx_min) / 2;
        my_ctr = (my_max + my_min) / 2;
        mz_ctr = (mz_max + mz_min) / 2;

        printf("x_min, max, ctr = %d %d %d\n", mx_min, mx_max, mx_ctr);
        printf("y_min, max, ctr = %d %d %d\n", my_min, my_max, my_ctr);
        printf("z_min, max, ctr = %d %d %d\n", mz_min, mz_max, mz_ctr);
        printf("CALIBRATION COMPLETE: %d %d %d\a\n", mx_ctr, my_ctr, mz_ctr);
    } else {
        mx_ctr =  45;   //  51
        my_ctr = -34;   // -34
        mz_ctr = -15;   // -11
        printf("USING DEFAULT CALIBRATION: %d %d %d\n", mx_ctr, my_ctr, mz_ctr);
    }

    // runtime
    while (true) {
        MPU9250_imu_get_magnetometer(&mx, &my, &mz);
        sleep(1);

        //heading = atan2(my-my_ctr, mx-mx_ctr) * (180 / M_PI);
        //if (heading < 0) heading += 360;

        heading = atan2(mx-mx_ctr, my-my_ctr) * (180 / M_PI);
        if (heading < 0) heading += 360;

        printf("HEADING %0.0f\n", heading);
    }
}

