#include "common.h"

static int drive_proc_0(void)
{
    if (drive_fwd(60, 1.00) < 0) return -1;
    if (drive_stop() < 0) return -1;

    return 0;
}

int (*drive_procs_tbl[100])(void) = {
    drive_proc_0
        };
