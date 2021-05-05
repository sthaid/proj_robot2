#include "common.h"

static int drive_proc_0(void);

int (*drive_procs_tbl[100])(void) = {
    drive_cal_proc,
    drive_proc_0
        };

// ------------------------------------------------------------------

static int drive_proc_0(void)
{
    int i;

    INFO("starting %s\n", __func__);

#if 0
    for (i = 0; i < 10; i++) {
        if (drive_fwd(1.20, 5) < 0) return -1;
        if (drive_rew(1.20, 5) < 0) return -1;
    }    
#endif
#if 0
    if (drive_xxx(2.2,1.2, 100) < 0) return -1;
#endif
#if 0
    double revs = 1;
    if (drive_rotate(1.2, revs * M_PI * 9.75 / 12 * 0.776) < 0) return -1;
#endif

    if (drive_fwd(1.20, 10) < 0) return -1;

    if (drive_stop() < 0) return -1;

    return 0;
}

