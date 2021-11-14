#include "common.h"

#define DONT_STOP_MOTORS  false
#define STOP_MOTORS       true

#define GET_ARG(n,default)  (dpm->arg[n] == 0 ? (default) : dpm->arg[n])
#define STEP(drvfunc) if ((drvfunc) < 0) return -1;

int drive_proc(struct msg_drive_proc_s *dpm)
{
    switch (dpm->proc_id) {
    case DRIVE_SCAL: {
        // scal [feet] - drive straight calibration
        double feet = GET_ARG(0, 5.0);  // default feet = 5
        STEP(drive_straight_cal(feet));
        break; }
    case DRIVE_MCAL: {
        // mcal [secs] - magnetometer calibration
        double num_rot = GET_ARG(0, 10);  // default num_rot = 10
        STEP(drive_mag_cal(num_rot));
        break; }

    case DRIVE_FWD: {
        // fwd [feet] [mph] - go straight forward
        double feet = GET_ARG(0, 5.0);   // default feet = 5
        double mph  = GET_ARG(1, 0.5);   // default mph  = 0.5
        STEP(drive_fwd(feet, mph, STOP_MOTORS));
        break; }
    case DRIVE_REV: {
        // rev [feet] [mph] - go straight reverse
        double feet = GET_ARG(0, 5.0);   // default feet = 5
        double mph  = GET_ARG(1, 0.5);   // default mph  = 0.5
        STEP(drive_rev(feet, mph));
        break; }
    case DRIVE_ROT: {
        // rot [degress] [fudge] - rotate 
        double degrees = GET_ARG(0, 180);  // default degrees = 180
        double fudge   = GET_ARG(1, 0);
        STEP(drive_rotate(degrees, fudge));
        break; }
    case DRIVE_HDG: {
        // hdg [heading] [fudge] - rotate to magnetic heading
        double heading = GET_ARG(0, 0);   // default heading = 0
        double fudge   = GET_ARG(1, 0);
        STEP(drive_rotate_to_heading(heading, fudge));
        break; }
    case DRIVE_RAD: {
        // rad [degrees] [radius_feet] [fudge] - drive forward with turn radius
        double degrees     = GET_ARG(0, 360);  // default degrees = 360
        double radius_feet = GET_ARG(1, 0);    // default radius  = 0 feet
        double fudge       = GET_ARG(2, 0);
        STEP(drive_radius(degrees, radius_feet, STOP_MOTORS, fudge));
        break; }

    case DRIVE_TST1: {
        // tst1 - repeat drive fwd/rev for range of mph, range 0.3 to 0.8 
        double mph;
        for (mph = 0.3; mph < 0.80001; mph += 0.1) {
            STEP(drive_fwd(5, mph, STOP_MOTORS));
            STEP(drive_rev(5, mph));
        }
        break; }
    case DRIVE_TST2: {
        // tst2 [fudge] - drive fwd, turn around and return to start point, using gyro
        double fudge = GET_ARG(0, 0);
        STEP(drive_fwd(5, 0.5, STOP_MOTORS));
        STEP(drive_rotate(180, fudge));
        STEP(drive_fwd(5, 0.5, STOP_MOTORS));
        STEP(drive_rotate(180, fudge));
        break; }
    case DRIVE_TST3: {
        // tst3 [fudge] - drive fwd, turn around and return to start point, using magnetometer
        double fudge = GET_ARG(0, 0);
        double curr_heading = imu_get_magnetometer();
        STEP(drive_fwd(5, 0.5, STOP_MOTORS));
        STEP(drive_rotate_to_heading(curr_heading+180, fudge));
        STEP(drive_fwd(5, 0.5, STOP_MOTORS));
        STEP(drive_rotate_to_heading(curr_heading, fudge));
        break; }
    case DRIVE_TST4: {
        // tst4 [cycles] [fudge] - repeating figure eight
        double cycles = GET_ARG(0, 3);   // default cycles = 3
        double fudge  = GET_ARG(1, 0);
        for (int i = 0; i < cycles; i++) {
            STEP(drive_radius(360, 1, DONT_STOP_MOTORS, fudge));
            STEP(drive_radius(-360, 1, DONT_STOP_MOTORS, fudge));
        }
        break; }
    case DRIVE_TST5: {
        // tst5 [cycles] [fudge] - repeating oval
        double cycles = GET_ARG(0, 3);   // default cycles = 3
        double fudge  = GET_ARG(1, 0);
        for (int i = 0; i < cycles; i++) {
            bool last = (i == cycles-1);
            STEP(drive_fwd(3, 0.5, DONT_STOP_MOTORS));
            STEP(drive_radius(180, 1, DONT_STOP_MOTORS, fudge));
            STEP(drive_fwd(3, 0.5, DONT_STOP_MOTORS));
            STEP(drive_radius(180, 1, last ? STOP_MOTORS : DONT_STOP_MOTORS, fudge));
        }
        break; }
    case DRIVE_TST6: {
        // tst6 [cycles] [fudge] - repeating square, corner turn radius = 1
        double cycles = GET_ARG(0, 3);   // default cycles = 3
        double fudge  = GET_ARG(1, 0);
        double corner_radius = 1;
        double feet = 1.5;
        for (int i = 0; i < cycles; i++) {
            bool last = (i == cycles-1);
            STEP(drive_fwd(feet, 0.5, DONT_STOP_MOTORS));
            STEP(drive_radius(90, corner_radius, DONT_STOP_MOTORS, fudge));
            STEP(drive_fwd(feet, 0.5, DONT_STOP_MOTORS));
            STEP(drive_radius(90, corner_radius, DONT_STOP_MOTORS, fudge));
            STEP(drive_fwd(feet, 0.5, DONT_STOP_MOTORS));
            STEP(drive_radius(90, corner_radius, DONT_STOP_MOTORS, fudge));
            STEP(drive_fwd(feet, 0.5, DONT_STOP_MOTORS));
            STEP(drive_radius(90, corner_radius, last ? STOP_MOTORS : DONT_STOP_MOTORS, fudge));
        }
        break; }
    case DRIVE_TST7: {
        // tst7 [cycles] [fudge] - repeating square, corner turn radius = 0
        double cycles = GET_ARG(0, 3);   // default cycles = 3
        double fudge  = GET_ARG(1, 0);
        double corner_radius = 0;
        double feet = 2;
        for (int i = 0; i < cycles; i++) {
            bool last = (i == cycles-1);
            STEP(drive_fwd(feet, 0.5, DONT_STOP_MOTORS));
            STEP(drive_radius(90, corner_radius, DONT_STOP_MOTORS, fudge));
            STEP(drive_fwd(feet, 0.5, DONT_STOP_MOTORS));
            STEP(drive_radius(90, corner_radius, DONT_STOP_MOTORS, fudge));
            STEP(drive_fwd(feet, 0.5, DONT_STOP_MOTORS));
            STEP(drive_radius(90, corner_radius, DONT_STOP_MOTORS, fudge));
            STEP(drive_fwd(feet, 0.5, DONT_STOP_MOTORS));
            STEP(drive_radius(90, corner_radius, last ? STOP_MOTORS : DONT_STOP_MOTORS, fudge));
        }
        break; }

    default:
        ERROR("invalid proc_id %d\n", dpm->proc_id);
        break;
    }

    return 0;
}
