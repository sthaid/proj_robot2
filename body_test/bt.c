// linux hdrs
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <curses.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

// body/include 
#include <body_network_intfc.h>

//
// defines
//

#define NODE "robot-body"

#define MAX_LOGMSG_STRS 50

// dest must be a char array, and not a char *
#define safe_strcpy(dest, src) \
    do { \
        strncpy(dest, src, sizeof(dest)-1); \
        (dest)[sizeof(dest)-1] = '\0'; \
    } while (0)

//
// variables
//

static int                 sfd;
static struct msg_status_s body_status;
static char                fatal_err_str[100];

static char                logmsg_strs[MAX_LOGMSG_STRS][MAX_LOGMSG_STR_SIZE];
static int                 logmsg_strs_count;

//
// prototypes
//

static void initialize(void);
static void error(char *fmt, ...) __attribute__ ((format (printf, 1, 2)));
static void fatal(char *fmt, ...) __attribute__ ((format (printf, 1, 2)));
static int getsockaddr(char * node, int port, struct sockaddr_in * ret_addr);
static char *sock_addr_to_str(char * s, int slen, struct sockaddr * addr);

static void *msg_receive_thread(void *cx);
static void send_msg(int id, void *data, int data_len);

static void update_display(int maxy, int maxx);
static int input_handler(int input_char);
static int  process_cmdline(void);

//
// curses wrapper definitions
//

#define COLOR_PAIR_RED   1
#define COLOR_PAIR_CYAN  2

static bool      curses_active;
static bool      curses_term_req;
static pthread_t curses_thread_id;
static WINDOW  * curses_window;

static void curses_init(void);
static void curses_exit(void);
static void curses_runtime(void (*update_display)(int maxy, int maxx), int (*input_handler)(int input_char));

// -----------------  MAIN & INITIALIZE & UTILS  ---------------------------------

int main(int argc, char **argv)
{
    // initialize
    initialize();

    // invoke the curses user interface
    curses_init();
    curses_runtime(update_display, input_handler);
    curses_exit();

    // if terminting due to fatal error then print the error str
    if (fatal_err_str[0] != '\0') {
        printf("%s\n", fatal_err_str);
        return 1;
    }

    // normal termination    
    return 0;
}

static void initialize(void)
{
    static struct sockaddr_in  sockaddr;
    char s[110];
    pthread_t tid;

    // set line buffered stdout
    setlinebuf(stdout);

    // get sockaddr for body pgm
    if (getsockaddr(NODE, PORT, &sockaddr) < 0) {
        fatal("failed to get address of %s", NODE);
    }

    // connect to body pgm
    sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (connect(sfd, (struct sockaddr*)&sockaddr, sizeof(sockaddr)) < 0) {
        fatal("failed connect to %s, %s",
              sock_addr_to_str(s, sizeof(s), (struct sockaddr *)&sockaddr),
              strerror(errno));
    }

    // create thread to receive and process msgs from body pgm
    pthread_create(&tid, NULL, msg_receive_thread, NULL);
}

static void error(char *fmt, ...)
{
    va_list ap;
    int cnt;
    char err_str[100];

    va_start(ap, fmt);
    cnt = sprintf(err_str, "BODY_TEST ERROR: ");
    cnt += vsnprintf(err_str+cnt, sizeof(err_str)-cnt, fmt, ap);
    va_end(ap);

    if (err_str[cnt-1] == '\n') {
        err_str[cnt-1] = '\0';
        cnt--;
    }

    if (curses_active == false) {
        printf("%s\n", err_str);
    } else {
        safe_strcpy(logmsg_strs[logmsg_strs_count%MAX_LOGMSG_STRS], err_str);
        __sync_fetch_and_add(&logmsg_strs_count, 1);
    }
}

static void fatal(char *fmt, ...)
{
    va_list ap;
    int cnt;

    va_start(ap, fmt);
    cnt = sprintf(fatal_err_str, "BODY_TEST FATAL: ");
    cnt += vsnprintf(fatal_err_str+cnt, sizeof(fatal_err_str)-cnt, fmt, ap);
    va_end(ap);

    if (fatal_err_str[cnt-1] == '\n') {
        fatal_err_str[cnt-1] = '\0';
        cnt--;
    }

    if (curses_active == false) {
        printf("%s\n", fatal_err_str);
        exit(1);
    } else if (pthread_self() == curses_thread_id) {
        curses_exit();
        printf("%s\n", fatal_err_str);
        exit(1);
    } else {
        __sync_synchronize();
        curses_term_req = true;
        while (true) pause();
    }
}

static int getsockaddr(char * node, int port, struct sockaddr_in * ret_addr)
{
    struct addrinfo   hints;
    struct addrinfo * result;
    char              port_str[20];
    int               ret;

    sprintf(port_str, "%d", port);

    bzero(&hints, sizeof(hints));
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = 0;
    hints.ai_flags    = AI_NUMERICSERV;

    ret = getaddrinfo(node, port_str, &hints, &result);
    if (ret != 0 || result->ai_addrlen != sizeof(*ret_addr)) {
        return -1;
    }

    *ret_addr = *(struct sockaddr_in*)result->ai_addr;
    freeaddrinfo(result);
    return 0;
}

static char * sock_addr_to_str(char * s, int slen, struct sockaddr * addr)
{
    char addr_str[100];
    int port;

    if (addr->sa_family == AF_INET) {
        inet_ntop(AF_INET,
                  &((struct sockaddr_in*)addr)->sin_addr,
                  addr_str, sizeof(addr_str));
        port = ((struct sockaddr_in*)addr)->sin_port;
    } else if (addr->sa_family == AF_INET6) {
        inet_ntop(AF_INET6,
                  &((struct sockaddr_in6*)addr)->sin6_addr,
                 addr_str, sizeof(addr_str));
        port = ((struct sockaddr_in6*)addr)->sin6_port;
    } else {
        snprintf(s,slen,"Invalid AddrFamily %d", addr->sa_family);
        return s;
    }

    snprintf(s,slen,"%s:%d",addr_str,htons(port));
    return s;
}

// -----------------  MSG RECIEVE THREAD  ----------------------------------------

static void *msg_receive_thread(void *cx)
{
    msg_t msg;
    int   rc;

    while (true) {
        // receive the msg.hdr
        rc = recv(sfd, &msg, sizeof(struct msg_hdr_s), MSG_WAITALL);
        if (rc != sizeof(struct msg_hdr_s)) {
            if (rc == 0) {
                fatal("connection closed by peer");
            } else {
                fatal("recv msg hdr rc=%d, %s", rc, strerror(errno));
            }
        }

        // validate the msg.hdr
        if (msg.hdr.magic != MSG_MAGIC ||
            msg.hdr.len < sizeof(struct msg_hdr_s) ||
            msg.hdr.len > sizeof(msg_t))
        {
            fatal("invalid msg magic 0x%x or len %d", msg.hdr.magic, msg.hdr.len);
        }

        // receive the remainder of the msg
        rc = recv(sfd, (void*)&msg+sizeof(struct msg_hdr_s), msg.hdr.len-sizeof(struct msg_hdr_s), MSG_WAITALL);
        if (rc != msg.hdr.len-sizeof(struct msg_hdr_s)) {
            if (rc == 0) {
                fatal("connection closed by peer");
            } else {
                fatal("recv msg rc=%d, %s", rc, strerror(errno));
            }
            exit(1);
        }

        // process the msg
        switch (msg.hdr.id) {
        case MSG_ID_STATUS:
            body_status = msg.status;
            break;
        case MSG_ID_LOGMSG:
            safe_strcpy(logmsg_strs[logmsg_strs_count%MAX_LOGMSG_STRS], msg.logmsg.str);
            __sync_fetch_and_add(&logmsg_strs_count, 1);
            break;
        default:
            fatal("unsupported msg id %d", msg.hdr.id);
            break;
        }
    }

    return NULL;
}

// -----------------  SEND MSG ---------------------------------------------------

static void send_msg(int id, void *data, int data_len)
{
    msg_t msg;
    int rc;

    // validate data_len
    if ((data == NULL && data_len != 0) ||
        (data != NULL && (data_len <= 0 || data_len > sizeof(msg_t)-sizeof(struct msg_hdr_s))))
    {
        fatal("data=%p data_len=%d", data, data_len);
    }

    // init msg_hdr
    msg.hdr.magic = MSG_MAGIC;
    msg.hdr.len   = sizeof(struct msg_hdr_s) + data_len;
    msg.hdr.id    = id;
    msg.hdr.pad   = 0;

    // copy data to msg, following the hdr
    if (data) {
        memcpy((void*)&msg+sizeof(struct msg_hdr_s), data, data_len);
    }

    // send the msg
    rc = send(sfd, &msg, msg.hdr.len, MSG_NOSIGNAL);   
    if (rc != msg.hdr.len) {
        if (rc == 0) {
            fatal("connection closed by peer");
        } else {
            fatal("send msg rc=%d, %s", rc, strerror(errno));
        }
    }
}

// -----------------  CURSES UPDATE_DISPLAY  -------------------------------

static char cmdline[100];

static void update_display(int maxy, int maxx)
{
    // display voltage and current
    // row 0
    mvprintw(0, 0,
             "VOLTAGE = %-5.2f - CURRENT = %-4.2f  (%4.2f + %4.2f)",
             body_status.voltage, 
             body_status.total_current, 
             body_status.electronics_current, 
             body_status.motors_current);

// xxx continue here
#if 0
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
#endif

    // display the logfile msgs
    // rows 19..maxy-5
    int num_rows = (maxy-5) - 19 + 1;
    int rcvd_count = logmsg_strs_count;
    for (int i = 0; i < num_rows; i++) {
        int idx = (rcvd_count-1) + i - (num_rows-1);
        if (idx < 0) continue;
        char *str = logmsg_strs[idx%MAX_LOGMSG_STRS];
        bool is_error_str = (strstr(str, "ERROR") != NULL);
        if (is_error_str) attron(COLOR_PAIR(COLOR_PAIR_RED));
        mvprintw(19+i, 0, "%s", str);
        if (is_error_str) attroff(COLOR_PAIR(COLOR_PAIR_RED));
    }

    // display cmdline
    mvprintw(maxy-1, 0, "> %s", cmdline);
}

// -----------------  CURSES INPUT_HANDLER  --------------------------------

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
    int cnt;
    int argc  __attribute__((unused));
    char cmd[100], arg1[100], arg2[100], arg3[100], arg4[100];

    cmd[0] = '\0';
    cnt = sscanf(cmdline, "%s %s %s %s %s", cmd, arg1, arg2, arg3, arg4);
    if (cnt == 0 || cmd[0] == '\0') {
        return 0;
    }
    argc = cnt - 1;

    // cmdline documentation:
    //   q              : terminate pgm
    //   cal            : perform motors calibration
    //   run <proc_id>  : run procedure proc_id
    //   mc_debug <0|1> : adjust mtr ctlr debug mode

    if (strcmp(cmd, "q") == 0) {
        return -1;  // terminate pgm
    } else if (strcmp(cmd, "cal") == 0) {
        send_msg(MSG_ID_DRIVE_CAL, NULL, 0);
    } else if (strcmp(cmd, "run") == 0) {
        struct msg_drive_proc_s x = { 0 };
        send_msg(MSG_ID_DRIVE_PROC, &x, sizeof(x));
    } else if (strcmp(cmd, "mc_debug") == 0) {
        int val;
        if (argc != 1 || sscanf(arg1, "%d", &val) != 1 || (val != 0 && val != 1)) {
            error("invalid arg1: %s", cmdline);
            return 0;
        }
        struct msg_mc_debug_ctl_s x = { val };
        send_msg(MSG_ID_MC_DEBUG_CTL, &x, sizeof(x));
    } else {
        error("invalid cmd: %s", cmdline);
    }

    return 0;
}
// -----------------  CURSES WRAPPER  ----------------------------------------

static void curses_init(void)
{
    curses_active = true;
    curses_thread_id = pthread_self();

    curses_window = initscr();

    start_color();
    use_default_colors();
    init_pair(COLOR_PAIR_RED, COLOR_RED, -1);
    init_pair(COLOR_PAIR_CYAN, COLOR_CYAN, -1);

    cbreak();
    noecho();
    nodelay(curses_window,TRUE);
    keypad(curses_window,TRUE);
}

static void curses_exit(void)
{
    endwin();

    curses_active = false;
}

static void curses_runtime(void (*update_display)(int maxy, int maxx), int (*input_handler)(int input_char))
{
    int input_char, maxy, maxx;
    int maxy_last=0, maxx_last=0;
    int sleep_us;

    while (true) {
        // erase display
        erase();

        // get window size, and print whenever it changes
        getmaxyx(curses_window, maxy, maxx);
        if (maxy != maxy_last || maxx != maxx_last) {
            maxy_last = maxy;
            maxx_last = maxx;
        }

        // update the display
        update_display(maxy, maxx);

        // refresh display
        refresh();

        // process character inputs; 
        // note ERR means 'no input is waiting'
        sleep_us = 0;
        input_char = getch();
        if (input_char == KEY_RESIZE) {
            // immedeate redraw display
        } else if (input_char != ERR) {
            if (input_char == 4) {  // 4 is ^d
                return;
            } else if (input_handler(input_char) != 0) {
                return;
            }
        } else {
            sleep_us = 100000;  // 100 ms
        }

        // if terminate curses request flag then return
        if (curses_term_req) {
            return;
        }

        // if need to sleep is indicated then do so
        if (sleep_us) {
            usleep(sleep_us);
        }
    }
}
