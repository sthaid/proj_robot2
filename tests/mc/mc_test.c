// XXX 
// add more cmds

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <curses.h>

#include <misc.h>
#include <mc.h>
#include <gpio.h>
#include <timer.h>
#include <encoder.h>

//
// defines
//

#define MAX_LOGMSG_STRS 128

#define COLOR_PAIR_RED   1
#define COLOR_PAIR_CYAN  2

//
// variables
//

static WINDOW * window;
static char     cmdline[100];
static char     alert_msg[100];
static uint64_t alert_msg_time_us;

static char     logmsg_strs[MAX_LOGMSG_STRS][200];
static int      logmsg_strs_tail;
static bool     logfile_monitor_thread_running;

//
// prototypes
//

static void *logfile_monitor_thread(void *cx);

static void update_display(int maxy, int maxx);
static int input_handler(int input_char);
static int  process_cmd(char *cmd);
static void display_alert(char *fmt, ...);

static void curses_init(void);
static void curses_exit(void);
static void curses_runtime(void (*update_display)(int maxy, int maxx), int (*input_handler)(int input_char));

// -----------------  MAIN  -------------------------------

#define LOG_FILENAME "mc_test.log"

int main(int argc, char **argv)
{
    pthread_t tid;

    // create thread to read the tail of logfile and copy
    // the new lines added to the logfile to logmsgs array
    pthread_create(&tid, NULL, logfile_monitor_thread, NULL);
    while (logfile_monitor_thread_running == false) {
        usleep(1000);
    }

    // initialize
    if (logmsg_init(LOG_FILENAME) < 0) {
        fprintf(stderr, "FATAL: logmsg_init failed, %s\n", strerror(errno));
        return 1;
    }
    if (gpio_init(true) < 0) {
        fprintf(stderr, "FATAL: gpio_init failed\n");
        return 1;
    }
    if (timer_init() < 0) {
        fprintf(stderr, "FATAL: timer_init failed\n");
        return 1;
    }
    if (encoder_init() < 0) {
        fprintf(stderr, "FATAL: encoder_init failed\n");
        return 1;
    }
    if (mc_init() < 0) {
        fprintf(stderr, "FATAL: mc_init failed\n");
        return 1;
    }

    // runtime using curses
    curses_init();
    curses_runtime(update_display, input_handler);
    curses_exit();
}

static void *logfile_monitor_thread(void *cx)
{
    FILE *fp;
    char s[200];

    // open the LOG_FILENAME for reading and seek to end
    fp = fopen(LOG_FILENAME, "r");
    if (fp == NULL) {
        fprintf(stderr, "FATAL: failed to open %s for reading, %s", LOG_FILENAME, strerror(errno));
        exit(1);
    }
    fseek(fp, 0, SEEK_END);
    logfile_monitor_thread_running = true;

    // loop forever reading from LOG_FILENAME and saving output to xxX
    while (true) {
        if (fgets(s, sizeof(s), fp) != NULL) {
            int new_tail = logmsg_strs_tail + 1;
            strncpy(logmsg_strs[new_tail], s, sizeof(logmsg_strs[new_tail%MAX_LOGMSG_STRS]));
            __sync_synchronize();
            logmsg_strs_tail = new_tail;
            continue;
        }
        clearerr(fp);
        usleep(10000);
    }
}

// --------------------------------------------------------

static void update_display(int maxy, int maxx)
{
    // display alert status for 5 secs
    // row 0
    if ((alert_msg_time_us != 0) &&
        (timer_get() - alert_msg_time_us < 5000000))
    {
        attron(COLOR_PAIR(COLOR_PAIR_RED));
        mvprintw(0, 40, "%s", alert_msg);
        attroff(COLOR_PAIR(COLOR_PAIR_RED));
    }

    // loop over all motor-ctlrs and display their variable values
    // rows: 1..4
    //             xxxxxxxxx xxxxxxxxx xxxxxxxxx xxxxxxxxx xxxxxxxxx xxxxxxxxx xxxxxxxxx xxxxxxxxx xxxxxxxxx xxxxxxxxx xxxxxxxxx xxxxxxxxx
    mvprintw(1,0, "  ERRSTAT  TGTSPEED CURRSPEED MAX_ACCEL MAX_DECEL       VIN   CURRENT CURRLIMIT   ENC_POS ENC_SPEED  ENC_ERRS ENC_POLLR");
    for (int id = 0; id < MAX_MC; id++) {
        int errstat, tgtspeed, currspeed, maxaccel, maxdecel, vin, current, currlimit;
        int enc_pos, enc_speed, enc_errs, enc_pollr;

        errstat   = 0x9999;
        tgtspeed  = 9999;
        currspeed = 9999;
        maxaccel  = 9999;
        maxdecel  = 9999;
        vin       = 9999;
        current   = 9999;
        currlimit = 9999;
        enc_pos   = 9999;
        enc_speed = 9999;
        enc_errs  = 9999;
        enc_pollr = 9999;

        mc_get_variable(id, VAR_ERROR_STATUS, &errstat);
        mc_get_variable(id, VAR_TARGET_SPEED, &tgtspeed);
        mc_get_variable(id, VAR_CURRENT_SPEED, &currspeed);
        mc_get_variable(id, VAR_MAX_ACCEL_FORWARD, &maxaccel);
        mc_get_variable(id, VAR_MAX_DECEL_FORWARD, &maxdecel);
        mc_get_variable(id, VAR_INPUT_VOLTAGE, &vin);
        mc_get_variable(id, VAR_CURRENT, &current);
        mc_get_variable(id, VAR_CURRENT_LIMITTING_OCCUR_CNT, &currlimit);

        encoder_get_ex(id, &enc_pos, &enc_speed, &enc_errs, &enc_pollr);

        mvprintw(3+id,0, "%9d %9d %9d %9d %9d %9d %9d %9d %9d %9d %9d %9d",
                errstat,
                tgtspeed, currspeed,
                maxaccel, maxdecel,
                vin, current, currlimit,
                enc_pos, enc_speed, enc_errs, enc_pollr);
    }

    // display the logfile msgs
    // rows 6..maxy-3
    int num_rows = (maxy-3) - 6 + 1;
    int tail = logmsg_strs_tail;
    for (int i = 0; i < num_rows; i++) {
        int idx = i + tail - num_rows + 1;
        if (idx < 0) continue;
        mvprintw(6+i, 0, "%s", logmsg_strs[idx%MAX_LOGMSG_STRS]);
    }

    // display cmdline
    // row maxy-1
    if ((timer_get() % 1000000) > 500000) {
        mvprintw(maxy-1, 0, "> %s", cmdline);
    } else {
        mvprintw(maxy-1, 0, "> %s_", cmdline);
    }
}

static int input_handler(int input_char)
{
    // xxx comment
    if (input_char == '\n') {
        display_alert("PROCESS CMD %s", cmdline); //xxx temp
        if (process_cmd(cmdline) == -1) {
            return -1;  // return -1, terminates pgm
        }
        memset(cmdline, 0, sizeof(cmdline));
    } else if (input_char == KEY_BACKSPACE) {
        int len = strlen(cmdline);
        if (len > 0) {
            cmdline[len-1] = '\0';
        }
    } else {
        int len = strlen(cmdline);
        cmdline[len] = input_char;
    }

    // return 0 means don't terminate pgm
    return 0;
}

// xxx more
static int process_cmd(char *cmd)
{
    if (strcmp(cmd, "enable") == 0) {
        mc_enable(0);
    } else if (strcmp(cmd, "fwd") == 0) {
        mc_speed(0, 2000);
    } else if (strcmp(cmd, "rev") == 0) {
        mc_speed(0, -2000);
    } else if (strcmp(cmd, "Fwd") == 0) {
        mc_speed(0, 3200);
    } else if (strcmp(cmd, "Rev") == 0) {
        mc_speed(0, -3200);
    } else if (strcmp(cmd, "accel") == 0) {
        mc_set_motor_limit(0, MTRLIM_MAX_ACCEL_FWD_AND_REV, 1);
        mc_set_motor_limit(0, MTRLIM_MAX_DECEL_FWD_AND_REV, 1);
    } else if (strcmp(cmd, "stop") == 0) {
        mc_stop(0);
    } else if (strcmp(cmd, "q") == 0) {
        return -1;
    } else {
    }

    return 0;

}

// display alert on top line of window 
static void display_alert(char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(alert_msg, sizeof(alert_msg), fmt, ap);
    va_end(ap);

    INFO("ALERT: %s\n", alert_msg);

    alert_msg_time_us = timer_get();
}

// -----------------  CURSES WRAPPER  ----------------------------------------

static void curses_init(void)
{
    window = initscr();

    start_color();
    use_default_colors();
    init_pair(COLOR_PAIR_RED, COLOR_RED, -1);
    init_pair(COLOR_PAIR_CYAN, COLOR_CYAN, -1);

    cbreak();
    noecho();
    nodelay(window,TRUE);
    keypad(window,TRUE);
}

static void curses_exit(void)
{
    endwin();
}

static void curses_runtime(void (*update_display)(int maxy, int maxx), int (*input_handler)(int input_char))
{
    int input_char, maxy, maxx;
    int maxy_last=0, maxx_last=0;

    while (true) {
        // erase display
        erase();

        // get window size, and print whenever it changes
        getmaxyx(window, maxy, maxx);
        if (maxy != maxy_last || maxx != maxx_last) {
            INFO("maxy=%d maxx=%d\n", maxy, maxx);
            maxy_last = maxy;
            maxx_last = maxx;
        }

        // update the display
        update_display(maxy, maxx);

        // put the cursor back to the origin, and
        // refresh display
        move(0,0);
        refresh();

        // process character inputs
        input_char = getch();
        if (input_char == KEY_RESIZE) {
            // immedeate redraw display
        } else if (input_char != ERR) {
            if (input_handler(input_char) != 0) {
                return;
            }
        } else {
            usleep(100000);
        }
    }
}

