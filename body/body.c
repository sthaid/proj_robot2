#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <curses.h>

#include <body.h>
#include <misc.h>

#include <gpio.h>
#include <timer.h>
#include <mc.h>
#include <encoder.h>
#include <proximity.h>
#include <button.h>
#include <current.h>
#include <oled.h>
#include <env.h>
#include <imu.h>

//
// defines
//

#define COLOR_PAIR_RED   1
#define COLOR_PAIR_CYAN  2

#define MAX_LOGMSG_STRS 128
#define LOG_FILENAME "mc_test.log"

//
// typedefs
//

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

static void init_devices(void);
static void init_logging(void);
static void *logfile_monitor_thread(void *cx);

static void update_display(int maxy, int maxx);
static int input_handler(int input_char);
static int  process_cmdline(void);
static void display_alert(char *fmt, ...) __attribute__((unused));

static void curses_init(void);
static void curses_exit(void);
static void curses_runtime(void (*update_display)(int maxy, int maxx), int (*input_handler)(int input_char));

// -----------------  MAIN AND INIT ROUTINES  ------------------------------

int main(int argc, char **argv)
{
    // init
    init_logging();
    init_devices();

    // runtime using curses
    curses_init();
    curses_runtime(update_display, input_handler);
    curses_exit();

    // done
    return 0;
}

static void init_devices(void)
{
    // XXX check rc,  exit on error
    gpio_init();
    timer_init();
    mc_init(2, LEFT_MOTOR, RIGHT_MOTOR);
    encoder_init(2, ENCODER_GPIO_LEFT_B, ENCODER_GPIO_LEFT_A,
                    ENCODER_GPIO_RIGHT_B, ENCODER_GPIO_RIGHT_A);
    proximity_init(2, PROXIMITY_FRONT_GPIO_SIG, PROXIMITY_FRONT_GPIO_ENABLE,
                    PROXIMITY_REAR_GPIO_SIG,  PROXIMITY_REAR_GPIO_ENABLE);
    button_init(2, BUTTON_LEFT, BUTTON_RIGHT);
    current_init(1, CURRENT_ADC_CHAN);
    oled_init(1, 0);
    env_init(0);
    imu_init(0);
}

static void init_logging(void)
{
    pthread_t tid;

    // create thread to read the tail of logfile and copy
    // the new lines added to the logfile to logmsgs array
    pthread_create(&tid, NULL, logfile_monitor_thread, NULL);
    while (logfile_monitor_thread_running == false) {
        usleep(1000);
    }

    // init logging to LOG_FILENAME
    if (logmsg_init(LOG_FILENAME) < 0) {
        fprintf(stderr, "FATAL: logmsg_init failed, %s\n", strerror(errno));
        exit(1);;
    }
}

static void *logfile_monitor_thread(void *cx)
{
    FILE *fp;
    char s[200];

    // open the LOG_FILENAME for reading and seek to end
    // XXX this failed if file not exists
    fp = fopen(LOG_FILENAME, "r");
    if (fp == NULL) {
        fprintf(stderr, "FATAL: failed to open %s for reading, %s\n", 
                LOG_FILENAME, strerror(errno));
        exit(1);
    }
    fseek(fp, 0, SEEK_END);
    logfile_monitor_thread_running = true;

    // loop forever reading from LOG_FILENAME and saving output to XXX
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

// -----------------  CURSES UPDATE_DISPLAY  -------------------------------

// XXX use curses alert (beep) routine

static void update_display(int maxy, int maxx)
{
    mc_status_t *mcs = mc_get_status();
    double electronics_current, motors_current, total_current;

    current_read(0, &electronics_current);
    motors_current = mcs->motors_current;
    total_current = electronics_current + motors_current;
    mvprintw(0, 0,
             "VOLTAGE = %-5.2f - CURRENT = %-4.2f  (%4.2f + %4.2f)",
             mcs->voltage, total_current, electronics_current, motors_current);

    // display the logfile msgs
    // rows 19..maxy-5
    int num_rows = (maxy-5) - 19 + 1;
    int tail = logmsg_strs_tail;
    for (int i = 0; i < num_rows; i++) {
        int idx = i + tail - num_rows + 1;
        if (idx < 0) continue;
        char *str = logmsg_strs[idx%MAX_LOGMSG_STRS];
        bool is_error_str = (strstr(str, "ERROR") != NULL);
        if (is_error_str) attron(COLOR_PAIR(COLOR_PAIR_RED));
        mvprintw(19+i, 0, "%s", str);
        if (is_error_str) attroff(COLOR_PAIR(COLOR_PAIR_RED));
    }

    // display alert status for 5 secs
    // - row maxy-3
    if ((alert_msg_time_us != 0) &&
        (microsec_timer() - alert_msg_time_us < 5000000))
    {
        attron(COLOR_PAIR(COLOR_PAIR_RED));
        mvprintw(maxy-3, 0, "%s", alert_msg);
        attroff(COLOR_PAIR(COLOR_PAIR_RED));
    }

    // display cmdline XXX can we leave the cursor here
    // - row maxy-1
    if ((microsec_timer() % 1000000) > 500000) {
        mvprintw(maxy-1, 0, "> %s", cmdline);
    } else {
        mvprintw(maxy-1, 0, "> %s_", cmdline);
    }
}

static void display_alert(char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(alert_msg, sizeof(alert_msg), fmt, ap);
    va_end(ap);

    alert_msg_time_us = microsec_timer();
}

// -----------------  CURSES INPUT_HANDLER  --------------------------------

static int input_handler(int input_char)
{
    // XXX comment
    if (input_char == '\n') {
        if (process_cmdline() == -1) {
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

static int process_cmdline(void)
{
    int cnt;
    char cmd[100];

    cmd[0] = '\0';
    cnt = sscanf(cmdline, "%s", cmd);
    if (cnt == 0 || cmd[0] == '\0') {
        return 0;
    }

    //INFO("cmd: %s\n", cmdline);

    if (strcmp(cmd, "q") == 0) {
        return -1;  // terminate pgm
    } else if (strcmp(cmd, "t1") == 0) {
        display_alert("alert test");
    } else if (strcmp(cmd, "mark") == 0) {
        INFO("----------------------------");
    } else {
        //display_alert("invalid cmd");
        ERROR("invalid cmd '%s'\n", cmdline);
    }

    return 0;
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
            //INFO("maxy=%d maxx=%d\n", maxy, maxx);
            maxy_last = maxy;
            maxx_last = maxx;
        }

        // update the display
        update_display(maxy, maxx);

        // put the cursor back to the origin, and
        // refresh display
        move(0,0);  // XXX maybe move this to handler 
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
            usleep(100000);  // 100 ms
        }
    }
}

#if 0

static bool     test_thread_is_running;

    // enable encoders
    for (int id = 0; id < 2; id++) {
        encoder_enable(id);
    }

static void update_display(int maxy, int maxx)
{

    // loop over all motor-ctlrs and display their variable values
    // rows: 1..4
    //             xxxxxxxxx xxxxxxxxx xxxxxxxxx xxxxxxxxx xxxxxxxxx xxxxxxxxx xxxxxxxxx xxxxxxxxx xxxxxxxxx xxxxxxxxx xxxxxxxxx xxxxxxxxx
    mvprintw(1,0, "  ERRSTAT  TGTSPEED CURRSPEED MAX_ACCEL MAX_DECEL       VIN   CURRENT CURRLIMIT   ENC_POS ENC_SPEED  ENC_ERRS  ENC_POLL");
    for (int id = 0; id < MAX_MC; id++) {
        int enc_pos, enc_speed, enc_errs, enc_poll_intvl_us;

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
        enc_poll_intvl_us = 9999;

#if 0
        mc_get_variable(id, VAR_ERROR_STATUS, &errstat);
        mc_get_variable(id, VAR_TARGET_SPEED, &tgtspeed);
        mc_get_variable(id, VAR_CURRENT_SPEED, &currspeed);
        mc_get_variable(id, VAR_MAX_ACCEL_FORWARD, &maxaccel);
        mc_get_variable(id, VAR_MAX_DECEL_FORWARD, &maxdecel);
        mc_get_variable(id, VAR_INPUT_VOLTAGE, &vin);
        mc_get_variable(id, VAR_CURRENT, &current);
        mc_get_variable(id, VAR_CURRENT_LIMITTING_OCCUR_CNT, &currlimit);
#endif

        encoder_get_position(id, &enc_pos);
        encoder_get_speed(id, &enc_speed);
        encoder_get_errors(id, &enc_errs);
        encoder_get_poll_intvl_us(&enc_poll_intvl_us);

        mvprintw(3+id,0, "%9d %9d %9d %9d %9d %9d %9d %9d %9d %9d %9d %9d",
                errstat,
                tgtspeed, currspeed,
                maxaccel, maxdecel,
                vin, current, currlimit,
                enc_pos, enc_speed, enc_errs, enc_poll_intvl_us);
    }


    // display test thread status
    // row mayx-2
    if (test_thread_is_running) {
        mvprintw(maxy-2, 0, "Test Thread Is Running");
    }
}

    char cmd[100];
    int  arg1, arg2, arg3;
    int  cnt;

    #define CHECK_SSCANF_CNT(num_expected_cnt, usage) \
        do { \
            if (cnt-1 != (num_expected_cnt)) { \
                ERROR("USAGE: %s %s\n", cmd, usage); \
                return 0; \
            } \
        } while (0)


    INFO("CMD: %s\n", cmdline);

    if (strcmp(cmd, "enable") == 0) {
        CHECK_SSCANF_CNT(1, "id");
        int id = arg1;
        mc_enable(id);
    } else if (strcmp(cmd, "speed") == 0) {
        CHECK_SSCANF_CNT(2, "id speed");
        int id = arg1;
        int speed = arg2;
        mc_speed(id, speed);
    } else if (strcmp(cmd, "accel") == 0) {
        CHECK_SSCANF_CNT(2, "id accel");
        int id = arg1;
        int accel = arg2;
        mc_set_motor_limit(id, MTRLIM_MAX_ACCEL_FWD_AND_REV, accel);
        mc_set_motor_limit(id, MTRLIM_MAX_DECEL_FWD_AND_REV, accel);
    } else if (strcmp(cmd, "enc_reset") == 0) {
        CHECK_SSCANF_CNT(1, "id");
        int id = arg1;
        encoder_pos_reset(id);
    } else if (strcmp(cmd, "stop") == 0) {
        CHECK_SSCANF_CNT(1, "id");
        int id = arg1;
        mc_stop(id);
    } else if (strcmp(cmd, "test") == 0) {
        CHECK_SSCANF_CNT(3, "id encoder_distance mtr_ctlr_speed");
        pthread_t tid;
        test_thread_cx_t *cx = malloc(sizeof(test_thread_cx_t));
        cx->id = arg1;
        cx->encoder_distance = arg2;
        cx->mtr_ctlr_speed = arg3;
        test_thread_is_running = true;
        pthread_create(&tid, NULL, test_thread, cx);
    return 0;

}

static void * test_thread(void *cx_arg)
{
    test_thread_cx_t *cx = cx_arg;
    int id       = cx->id;                // mc id
    int distance = cx->encoder_distance;  // encoder distance
    int speed    = cx->mtr_ctlr_speed;    // mc speed
    int pos_at_end_of_accel;
    int p;

    // free cx_arg
    free(cx_arg);

    // reset encode position
    encoder_pos_reset(id);

    // set desired speed
    mc_speed(id, speed);

    // delay for the acceleration interval, and
    // get the encoder position  XXX assumes accel = 1
    usleep(speed*1000);
    encoder_get_position(0, &pos_at_end_of_accel);
    INFO("xxx pos at end of accel %d\n", pos_at_end_of_accel);
    // XXX need to poll here for short distances

    // wait for position to be 'distance - pos_at_end_of_accel'
    while (true) {
        encoder_get_position(0, &p);
        if (p > distance - pos_at_end_of_accel - 650) {
            break;
        }
        usleep(1000);
    }

    // set speed to 0
    INFO("pos at begining of decel %d\n", p);
    mc_speed(id, 0);

    // XXX could wait for speed 0, and print deviation

    // test thread is done
    test_thread_is_running = false;
    return NULL;
}

// display alert on top line of window 


#endif
