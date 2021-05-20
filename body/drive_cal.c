// XXX move to drive.c
#include "common.h"

//
// defines
//

#define MAX_DRIVE_CAL_TBL   30
#define DRIVE_CAL_FILENAME  "drive.cal"

//
// variables
//

static struct drive_cal_s {
    double mph;
    int    lspeed;
    int    rspeed;
} drive_cal_tbl[MAX_DRIVE_CAL_TBL];

//
// prototypes
//

static void drive_cal_tbl_init_default(void);

// -----------------  DRIVE CALIBRATION  -------------------------------------

int drive_cal_file_read(void)
{
    FILE *fp;
    int idx=0;
    char s[100];

    fp = fopen(DRIVE_CAL_FILENAME, "r");
    if (fp == NULL) {
        WARN("failed to open drive.cal, using default drive_cal_tbl, %s\n", strerror(errno));
        drive_cal_tbl_init_default();
        return 0;
    }
    while (fgets(s, sizeof(s), fp)) {
        if (idx == MAX_DRIVE_CAL_TBL) {
            ERROR("too man entries in %s\n", DRIVE_CAL_FILENAME);
            return -1;
        }

        struct drive_cal_s *x = &drive_cal_tbl[idx];
        if (sscanf(s, "%lf %d %d", &x->mph, &x->lspeed, &x->rspeed)!= 3) {
            ERROR("invalid line: %s\n", s);
            return -1;
        }
        idx++;
    }
    fclose(fp);

    return 0;
}

int drive_cal_file_write(void)
{
    FILE *fp;
    int idx;

    fp = fopen(DRIVE_CAL_FILENAME, "w");
    if (fp == NULL) {
        ERROR("failed to open drive.cal for writing, %s\n", strerror(errno));
        return -1;
    }
    for (idx = 0; idx < MAX_DRIVE_CAL_TBL; idx++) {
        struct drive_cal_s *x = &drive_cal_tbl[idx];
        if (x->mph == 0) {
            break;
        }
        fprintf(fp, "%4.1f %5d %5d\n", x->mph, x->lspeed, x->rspeed);
    }
    fclose(fp);

    return 0;
}

void drive_cal_tbl_print(void)
{
    int idx;

    INFO(" MPH LSPEED RSPEED\n");
    INFO(" --- ------ ------\n");
    for (idx = 0; idx < MAX_DRIVE_CAL_TBL; idx++) {
        struct drive_cal_s *x = &drive_cal_tbl[idx];
        if (x->mph == 0) {
            break;
        }
        INFO("%4.1f %6d %6d\n", x->mph, x->lspeed, x->rspeed);
    }
}

int drive_cal_proc(void)
{
    int idx, lspeed, rspeed;
    struct drive_cal_s new_drive_cal_tbl[MAX_DRIVE_CAL_TBL];

    // XXX try cal over longer distance
    #define CAL_FEET 5

    // make copy of drive_cal_tbl
    memcpy(new_drive_cal_tbl, drive_cal_tbl, sizeof(drive_cal_tbl));

    // loop over the new_drive_cal_tbl, 
    // and update the lspeed and rspeed values
    for (idx = 0; idx < MAX_DRIVE_CAL_TBL; idx++) {
        struct drive_cal_s *new = &new_drive_cal_tbl[idx];
        if (new->mph == 0) {
            break;
        }

        INFO("calibrate mph %0.1f\n", new->mph);
        if (drive_straight(CAL_FEET, new->mph, &lspeed, &rspeed) < 0) {
            ERROR("drive_straight failed\n");
            break;
        }
        new->lspeed = lspeed;
        new->rspeed = rspeed;
    }

    // print the current and new drive_cal_tbl values
    INFO("          CURRENT           NEW            DELTA\n");
    INFO(" MPH   LSPEED RSPEED   LSPEED RSPEED   LSPEED RSPEED\n");
    INFO(" ---   ------ ------   ------ ------   ------ ------\n");
    for (idx = 0; idx < MAX_DRIVE_CAL_TBL; idx++) {
        struct drive_cal_s *cur = &drive_cal_tbl[idx];
        struct drive_cal_s *new = &new_drive_cal_tbl[idx];
        if (new->mph == 0) {
            break;
        }
        INFO("%4.1f   %6d %6d   %6d %6d   %6d %6d\n", 
             cur->mph, 
             cur->lspeed, cur->rspeed,
             new->lspeed, new->rspeed,
             new->lspeed-cur->lspeed, new->rspeed-cur->rspeed);
    }

    // publish the new_drive_cal_tbl to drive_cal_tbl,
    // write drive_cal_tbl to file
    memcpy(drive_cal_tbl, new_drive_cal_tbl, sizeof(drive_cal_tbl));
    drive_cal_file_write();
    return 0;
}

void drive_cal_cvt_mph_to_mtr_speeds(double mph, int *lspeed, int *rspeed)
{
    int idx, min_idx=-1;
    double min_delta = 1000000;
    double delta;

    // loop over the drive_cal_tbl and find the entry with mph nearest
    // what caller is requesting
    for (idx = 0; idx < MAX_DRIVE_CAL_TBL; idx++) {
        struct drive_cal_s *x = &drive_cal_tbl[idx];
        if (x->mph == 0) {
            break;
        }
        delta = fabs(mph - x->mph);
        if (delta < min_delta) {
            min_delta = delta;
            min_idx = idx;
        }
    }

    // return the lspeed and rspeed
    if (idx == -1) {
        FATAL("bug\n");
    }
    *lspeed = drive_cal_tbl[min_idx].lspeed;
    *rspeed = drive_cal_tbl[min_idx].rspeed;

    // debug print result
    INFO("requested mph = %0.1f  nearest_mph = %0.1f  l/rspeed = %d %d\n",
         mph, drive_cal_tbl[min_idx].mph, *lspeed, *rspeed);
}

static void drive_cal_tbl_init_default(void)
{
    int n, idx = 0;
    struct drive_cal_s *x;

    // init drive_cal_tbl to default, in case the drive cal file doesn't exist
    for (n = 3; n <= 8; n++) {
        double mph = 0.1 * n;

        // add entries for mph
        x = &drive_cal_tbl[idx];
        x->mph = mph;
        x->lspeed = x->rspeed = MTR_MPH_TO_SPEED(x->mph);
        idx++;

        // add entries for -mph
        x = &drive_cal_tbl[idx];
        x->mph = -mph;
        x->lspeed = x->rspeed = MTR_MPH_TO_SPEED(x->mph);
        idx++;
    }
}

