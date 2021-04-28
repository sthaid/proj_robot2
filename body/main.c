#include "common.h"

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

static bool     mc_debug_mode_enabled;

static bool     sigint;

//
// prototypes
//

static void initialize(void);
static void sigint_hndlr(int signum);
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
    initialize();

    // invoke the curses user interface
    curses_init();
    curses_runtime(update_display, input_handler);
    curses_exit();

    // stop motors
    mc_disable_all();

    // done
    return 0;
}

static void initialize(void)
{
    pthread_t tid;

    #define CALL(routine,args) \
        do { \
            int rc = routine args; \
            if (rc < 0) { \
                fprintf(stderr, "FATAL: %s failed\n", #routine); \
                exit(1); \
            } \
        } while (0)

    // init logging to LOG_FILENAME
    if (logmsg_init(LOG_FILENAME) < 0) {
        fprintf(stderr, "FATAL: logmsg_init failed, %s\n", strerror(errno));
        exit(1);;
    }

    // create thread to read the tail of logfile and copy
    // the new lines added to the logfile to logmsgs array
    pthread_create(&tid, NULL, logfile_monitor_thread, NULL);
    while (logfile_monitor_thread_running == false) {
        usleep(1000);
    }

    // init devices
    CALL(gpio_init, ());
    CALL(timer_init, ());
    CALL(mc_init, (2, LEFT_MOTOR, RIGHT_MOTOR));
    CALL(encoder_init, (2, ENCODER_GPIO_LEFT_B, ENCODER_GPIO_LEFT_A,
                           ENCODER_GPIO_RIGHT_B, ENCODER_GPIO_RIGHT_A));
    CALL(proximity_init, (2, PROXIMITY_FRONT_GPIO_SIG, PROXIMITY_FRONT_GPIO_ENABLE,
                             PROXIMITY_REAR_GPIO_SIG,  PROXIMITY_REAR_GPIO_ENABLE));
    CALL(button_init, (2, BUTTON_LEFT, BUTTON_RIGHT));
    CALL(current_init, (1, CURRENT_ADC_CHAN));
    CALL(oled_init, (1, 0));
    CALL(env_init, (0));
    CALL(imu_init, (0));

    // init body program functions
    CALL(oled_ctlr_init, ());
    CALL(drive_init, ());

    // register SIGINT
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = sigint_hndlr;
    sigaction(SIGINT, &act, NULL);
}

static void sigint_hndlr(int signum)
{
    sigint = true;
}

static void *logfile_monitor_thread(void *cx)
{
    FILE *fp;
    char s[200];

    // open the LOG_FILENAME for reading and seek to end
    fp = fopen(LOG_FILENAME, "r");
    if (fp == NULL) {
        fprintf(stderr, "FATAL: failed to open %s for reading, %s\n", 
                LOG_FILENAME, strerror(errno));
        exit(1);
    }
    fseek(fp, 0, SEEK_END);
    logfile_monitor_thread_running = true;

    // loop forever reading from LOG_FILENAME and saving output to 
    // logmsg_strs[] circular array of strings
    while (true) {
        if (fgets(s, sizeof(s), fp) != NULL) {
            int new_tail = logmsg_strs_tail + 1;
            strncpy(logmsg_strs[new_tail%MAX_LOGMSG_STRS], s, sizeof(logmsg_strs[0]));
            __sync_synchronize();
            logmsg_strs_tail = new_tail;
            continue;
        }
        clearerr(fp);
        usleep(10000);
    }
}

// -----------------  CURSES UPDATE_DISPLAY  -------------------------------

static void update_display(int maxy, int maxx)
{
    mc_status_t *mcs = mc_get_status();
    double electronics_current, motors_current, total_current;
    int id;

    // display voltage and current
    // row 0
    electronics_current = current_read_smoothed(0);
    motors_current = mcs->motors_current;
    total_current = electronics_current + motors_current;
    mvprintw(0, 0,
             "VOLTAGE = %-5.2f - CURRENT = %-4.2f  (%4.2f + %4.2f)",
             mcs->voltage, total_current, electronics_current, motors_current);

    // display motor ctlr values
    // rows 2-5
    mvprintw(2, 0,
             "MOTORS: %s   EncPollIntvlUs=%d",
            MC_STATE_STR(mcs->state), encoder_get_poll_intvl_us());
    if (mc_debug_mode_enabled) {
        mvprintw(3,0, 
             "      Target   Ena Position Speed Errors   ErrStat Target Current Accel Voltage Current");
    } else {
        mvprintw(3,0, 
             "      Target   Ena Position Speed Errors");
    }
    for (id = 0; id < 2; id++) {
        if (mc_debug_mode_enabled) {
            struct debug_mode_mtr_vars_s *x = &mcs->debug_mode_mtr_vars[id];
            mvprintw(4+id, 0, 
                     "  %c - %6d   %3d %8d %5d %6d    0x%4.4x %6d %7d %2d %2d %7.2f %7.2f",
                     (id == 0 ? 'L' : 'R'), mcs->target_speed[id],
                     encoder_get_enabled(id),
                     encoder_get_position(id),
                     encoder_get_speed(id),
                     encoder_get_errors(id),
                     x->error_status, x->target_speed, x->current_speed, 
                     x->max_accel, x->max_decel,
                     x->input_voltage/1000., x->current/1000.);
        } else {
            mvprintw(4+id, 0, 
                     "  %c - %6d   %3d %8d %5d %6d",
                     (id == 0 ? 'L' : 'R'), mcs->target_speed[id],
                     encoder_get_enabled(id),
                     encoder_get_position(id),
                     encoder_get_speed(id),
                     encoder_get_errors(id));
        }
    }

    // display proximity sensor values
    // rows 7-9
    mvprintw(7, 0,
             "PROXIMITY:  AlertSigLimit=%4.2f  PollIntvlUs=%d",
             proximity_get_alert_limit(), proximity_get_poll_intvl_us());
    for (id = 0; id < 2; id++) {
        double proximity_avg_sig;
        bool proximity_alert;

        proximity_alert = proximity_check(id, &proximity_avg_sig);
        mvprintw(8+id, 0,
                 "  %c - Enabled=%d  Alert=%d  Sig=%4.2f",
                 (id == 0 ? 'F' : 'R'),
                 proximity_get_enabled(id),
                 proximity_alert,
                 proximity_avg_sig);
        if (proximity_alert) {
            beep();
        }
    }

    // display IMU values
    // row 11
    static double last_accel_alert_value;
    static int    accel_alert_count;
    double accel_alert_value;
    if (imu_check_accel_alert(&accel_alert_value)) {
        last_accel_alert_value = accel_alert_value;
        accel_alert_count++;
        beep();
    }
    mvprintw(11, 0,
        "IMU:  Heading=%3.0f - Accel Ena=%d AlertCount=%d LastAlertValue=%4.2f AlertValueLimit=%4.2f",
        imu_read_magnetometer(),
        imu_get_accel_enabled(),
        accel_alert_count, last_accel_alert_value,
        imu_get_accel_alert_limit());

    // display ENV values
    // row 13
    double temperature, pressure;
    temperature = env_read_temperature_degc();
    pressure = env_read_pressure_pascal();
    mvprintw(13, 0, 
        "ENV:  %4.1f C  %0.0f Pa - %4.1f F  %5.2f in Hg",
        temperature, pressure, 
        CENTIGRADE_TO_FAHRENHEIT(temperature),
        PASCAL_TO_INHG(pressure));

    // button values
    // row 15
    mvprintw(15, 0, 
        "BTNS: %d  %d",
        button_is_pressed(0),
        button_is_pressed(1));

    // oled strings
    // row 17
    int i;
    oled_strs_t *strs;
    strs = oled_get_strs();
    mvprintw(17, 0, "OLED:");
    for (i = 0; i < MAX_OLED_STR; i++) {
        mvprintw(17, 6+10*i, (*strs)[i]);
    }

    // display the logfile msgs
    // rows 19..maxy-5
    int num_rows = (maxy-5) - 19 + 1;
    int tail = logmsg_strs_tail;
    for (int i = 0; i < num_rows; i++) {
        int idx = i + tail - num_rows + 1;
        if (idx <= 0) continue;
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

    // display cmdline
    mvprintw(maxy-1, 0, "> %s", cmdline);
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

// xxx add recall command history
static int input_handler(int input_char)
{
    // process input_char
    if (input_char == '\n') {
        if (process_cmdline() == -1) {
            return -1;  // terminates pgm
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
    int cnt, argc;
    char cmd[100], arg1[100], arg2[100], arg3[100], arg4[100];

    cmd[0] = '\0';
    cnt = sscanf(cmdline, "%s %s %s %s %s", cmd, arg1, arg2, arg3, arg4);
    if (cnt == 0 || cmd[0] == '\0') {
        return 0;
    }
    argc = cnt - 1;

    INFO("cmd: %s\n", cmdline);

    if (strcmp(cmd, "q") == 0) {
        return -1;  // terminate pgm
    } else if (strcmp(cmd, "cal") == 0) {
        drive_run_cal();
    } else if (strcmp(cmd, "run") == 0) {
        drive_run_proc(0);  // xxx arg needed
    } else if (strcmp(cmd, "mc_debug_on") == 0) {
        mc_debug_mode(true);
        mc_debug_mode_enabled = true;
    } else if (strcmp(cmd, "mc_debug_off") == 0) {
        mc_debug_mode(false);
        mc_debug_mode_enabled = false;
    } else if (strcmp(cmd, "mark") == 0) {
        INFO("------------------------------------");
    } else {
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

        // refresh display
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

        // return if sigint (aka ctrl-c)
        if (sigint) {
            INFO("exit due to ctrl-c\n");
            return;
        }
    }
}
