#include "common.h"

#define GET_ARG(n,default)  (dpm->arg[n] == 0 ? (default) : dpm->arg[n])
#define STEP(drvfunc) if ((drvfunc) < 0) return -1;

int drive_proc(struct msg_drive_proc_s *dpm)
{
    switch (dpm->proc_id) {
    case 1: {
        // drive straight calibration
        STEP(drive_straight_cal());
        break; }
    case 2: {
        // magnetometer calibration
        STEP(drive_mag_cal());
        break; }
    case 5: {
        // forward
        double feet = GET_ARG(0, 5.0);
        double mph  = GET_ARG(1, 0.5);
        STEP(drive_fwd(feet, mph));
        break; }
    case 6: {
        // reverse
        double feet = GET_ARG(0, 5.0);
        double mph  = GET_ARG(1, 0.5);
        STEP(drive_rev(feet, mph));
        break; }
    case 7: {
        // rotate
        double degrees = GET_ARG(0, 180);
        double fudge   = GET_ARG(1, 0);
        STEP(drive_rotate(degrees, fudge));
        break; }
    case 8: {
        // rotate to magnetic heading
        double heading = GET_ARG(0, 0);
        double fudge   = GET_ARG(1, 0);
        STEP(drive_rotate_to_heading(heading, fudge, false));
        break; }
    case 9: {
        // drive forward with turn radius
        double degrees     = GET_ARG(0, 360);
        double radius_feet = GET_ARG(1, 0);
        double fudge       = GET_ARG(2, 0);
        STEP(drive_radius(degrees, radius_feet, fudge));
        break; }
    case 21: {
        // repeat drive fwd/rev for range of mph
        double mph;
        for (mph = 0.3; mph < 0.80001; mph += 0.1) {
            STEP(drive_fwd(5, mph));
            STEP(drive_rev(5, mph));
        }
        break; }
    case 22: {
        // drive fwd, turn around and return to start point, using gyro
        STEP(drive_fwd(5, 0.5));
        STEP(drive_rotate(180, 0));
        STEP(drive_fwd(5, 0.5));
        STEP(drive_rotate(180, 0));
        break; }
    case 23: {
        // drive fwd, turn around and return to start point, using magnetometer
        double curr_heading = imu_get_magnetometer();
        STEP(drive_fwd(5, 0.5));
        STEP(drive_rotate_to_heading(curr_heading+180, 0, false));
        STEP(drive_fwd(5, 0.5));
        STEP(drive_rotate_to_heading(curr_heading, 0, false));
        break; }
    default:
        ERROR("invalid proc_id %d\n", dpm->proc_id);
        break;
    }

    return 0;
}
