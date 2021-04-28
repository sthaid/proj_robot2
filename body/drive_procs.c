#include "common.h"

static int drive_proc_0(void);

int (*drive_procs_tbl[100])(void) = {
    drive_proc_0
        };

// ------------------------------------------------------------------

static int drive_proc_0(void)
{
    if (drive_fwd(60*1000000, 1.20) < 0) return -1;
    if (drive_stop() < 0) return -1;

    return 0;
}

