#include "common.h"

/*
int drive_fwd(double feet, double mph);
int drive_rew(double feet, double mph);
int drive_rotate(double degrees, double rpm);
int drive_stop(void);
*/

#define GET_ARG(n,default)  (dpm->arg[n] == 0 ? (default) : dpm->arg[n])
#define STEP(drvfunc) if ((drvfunc) < 0) return -1;

int drive_proc(struct msg_drive_proc_s *dpm)
{
    switch (dpm->proc_id) {
    case 1: {
        if (drive_cal_proc() < 0) return -1;
        break; }
    case 2: {
        double feet = GET_ARG(0, 6.0);
        double mph  = GET_ARG(1, 1.1);
        STEP(drive_fwd(feet, mph));
        STEP(drive_stop());
        break; }
    case 3: {
        double feet = GET_ARG(0, 1.0);
        double mph  = GET_ARG(1, 1.1);
        STEP(drive_rev(feet, mph));
        STEP(drive_stop());
        break; }
    default:
        ERROR("invalid proc_id %d\n", dpm->proc_id);
        break;
    }

    return 0;
}
