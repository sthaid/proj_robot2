// xxx msg to abort a drive proc, and possibly when a client exits

#include "common.h"

//
// defines
//

#define MAX_LOGMSG_STRS      50
#define MAX_LOGMSG_STR_SIZE  100

// dest must be a char array, and not a char *
#define safe_strcpy(dest, src) \
    do { \
        strncpy(dest, src, sizeof(dest)-1); \
        (dest)[sizeof(dest)-1] = '\0'; \
    } while (0)

//
// typedefs
//

//
// variables
//

static char logmsg_strs[MAX_LOGMSG_STRS][MAX_LOGMSG_STR_SIZE];
static int  logmsg_strs_count;

static bool sigint, sigterm;

//
// prototypes
//

static void initialize(void);
static void sig_hndlr(int signum);
static void logmsg_cb(char *str);

static void server(void);
static void *server_thread(void *cx);
static int send_msg(int sockfd, int id, void *data, int data_len);
static struct msg_status_s * generate_msg_status(void);
static void process_received_msg(msg_t *msg);

// -----------------  MAIN AND INIT ROUTINES  ------------------------------

int main(int argc, char **argv)
{
    // init
    initialize();

    // call server to accept and process connections from clients
    server();

    // stop motors
    drive_emer_stop();

    // print reason for terminating
    if (sigint || sigterm) {
        INFO("terminating due to %s\n", sigint ? "SIGINT" : "SIGTERM");
    }

    // call oled_exit to clear the oled display
    oled_ctlr_exit("term");

    // done
    return 0;
}

static void initialize(void)
{
    //pthread_t tid;

    #define CALL(routine,args) \
        do { \
            int rc = routine args; \
            if (rc < 0) { \
                fprintf(stderr, "FATAL: %s failed\n", #routine); \
                exit(1); \
            } \
        } while (0)

    // register logmsg callback
    logmsg_register_cb(logmsg_cb);

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

    // register SIGTERM
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = sig_hndlr;
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGTERM, &act, NULL);
}

static void sig_hndlr(int signum)
{
    if (signum == SIGINT) sigint = true;
    if (signum == SIGTERM) sigterm = true;
}

static void logmsg_cb(char *str)
{
    safe_strcpy(logmsg_strs[logmsg_strs_count%MAX_LOGMSG_STRS], str);
    __sync_fetch_and_add(&logmsg_strs_count,1);
}

// -----------------  SERVER  ----------------------------------------------

static void server(void)
{
    struct sockaddr_in server_address;
    int32_t            listen_sockfd;
    int32_t            ret;
    pthread_t          thread;
    pthread_attr_t     attr;
    int32_t            optval;
    char               s[200];

    // create socket
    listen_sockfd = socket(AF_INET, SOCK_STREAM, 0);

    // set reuseaddr
    optval = 1;
    setsockopt(listen_sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, 4);

    // bind socket
    bzero(&server_address, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(PORT);
    ret = bind(listen_sockfd,
               (struct sockaddr *)&server_address,
               sizeof(server_address));
    if (ret == -1) {
        FATAL("bind, %s\n", strerror(errno));
    }

    // listen 
    listen(listen_sockfd, 2);

    // init thread attributes to make thread detached
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    // loop, accepting connection, and create thread to service the client
    INFO("server: accepting connections\n");
    while (1) {
        int                sockfd;
        socklen_t          len;
        struct sockaddr_in address;

        // accept connection
        len = sizeof(address);
        sockfd = accept(listen_sockfd, (struct sockaddr *) &address, &len);
        if (sockfd == -1) {
            if (sigint || sigterm) {
                break;
            }
            ERROR("accept, %s\n", strerror(errno));
            continue;
        }

        // create thread
        INFO("accepted connection from %s\n", sock_addr_to_str(s,sizeof(s),(struct sockaddr *)&address));
        pthread_create(&thread, &attr, server_thread, (void*)(uintptr_t)sockfd);
    }
}

static void * server_thread(void * cx)
{
    int      sockfd = (uintptr_t)cx;
    msg_t    recv_msg;
    uint64_t time_last_send = microsec_timer();
    uint64_t time_now;
    int      rc;
    int      recvd_len = 0;
    int      logmsg_strs_sent_count;

    // init logmsg_strs_sent_count, so that the most recent 10 logmsg strs 
    // will be sent to the client
    logmsg_strs_sent_count = logmsg_strs_count - 10;
    if (logmsg_strs_sent_count < 0) {
        logmsg_strs_sent_count = 0;
    }

    while (true) {
        // perform non-blocking recv of msg, this is accomplished by
        // first receiving the hdr (which includes the msg len), and then
        // receiving the rest of the msg; and
        // when the msg is recieved, call process_received_msg
        if (recvd_len < sizeof(struct msg_hdr_s)) {
            // call recv to recieve the msg hdr
            rc = recv(sockfd, (void*)&recv_msg+recvd_len, sizeof(struct msg_hdr_s)-recvd_len, MSG_DONTWAIT);
        } else {
            // sanity check the msg_hdr magic and len fields, 
            // if invalid then terminate connection
            if (recv_msg.hdr.magic != MSG_MAGIC || 
                recv_msg.hdr.len < sizeof(struct msg_hdr_s) || 
                recv_msg.hdr.len > sizeof(recv_msg)) 
            {
                ERROR("invalid msg magic 0x%x or len %d\n", recv_msg.hdr.magic, recv_msg.hdr.len);
                goto term;
            }
            // call recv to receive the remainder of the msg
            rc = recv(sockfd, (void*)&recv_msg+recvd_len, recv_msg.hdr.len-recvd_len, MSG_DONTWAIT);
        }
        if (rc > 0) {
            recvd_len += rc;
            if (recvd_len >= sizeof(struct msg_hdr_s) && recvd_len == recv_msg.hdr.len) {
                process_received_msg(&recv_msg);
                memset(&recv_msg, 0, recvd_len);
                recvd_len = 0;
            }
        } else if (rc == 0) {
            // terminate connection
            INFO("connection terminated by peer\n");
            goto term;
        } else if (errno != EWOULDBLOCK && errno != EAGAIN) {
            // errno is unexpected, print msg and terminate connection
            ERROR("recv failed, %s\n", strerror(errno));
            goto term;
        }

        // if it has been 100 ms since last sending status msg then do so now
        time_now = microsec_timer();
        if (time_now - time_last_send > 100000) {
            if (send_msg(sockfd, MSG_ID_STATUS, generate_msg_status(),
                         sizeof(struct msg_status_s)) < 0) {
                goto term;
            }
            time_last_send = time_now;
        }

        // if there are logmsg_strs that need to be sent, then do so
        while (logmsg_strs_sent_count < logmsg_strs_count) {
            char *str = logmsg_strs[logmsg_strs_sent_count % MAX_LOGMSG_STRS];
            if (send_msg(sockfd, MSG_ID_LOGMSG, str, strlen(str)+1) < 0) {
                goto term;
            }
            logmsg_strs_sent_count++;
        }

        // short sleep 
        usleep(25000);  // 25 ms
    }

term:
    drive_emer_stop();
    close(sockfd);
    return NULL;
}

static int send_msg(int sockfd, int id, void *data, int data_len)
{
    msg_t msg;
    int rc;

    // validate data_len
    if ((data == NULL && data_len != 0) ||
        (data != NULL && (data_len <= 0 || data_len > sizeof(msg_t)-sizeof(struct msg_hdr_s))))
    {
        FATAL("BUG data=%p data_len=%d", data, data_len);
    }

    // init the msg.hdr fields
    msg.hdr.magic = MSG_MAGIC;
    msg.hdr.len   = sizeof(struct msg_hdr_s) + data_len;
    msg.hdr.id    = id;
    msg.hdr.pad   = 0;

    // copy data to msg, following the hdr
    if (data) {
        memcpy((void*)&msg+sizeof(struct msg_hdr_s), data, data_len);
    }

    // send the msg
    rc = send(sockfd, &msg, msg.hdr.len, MSG_NOSIGNAL);
    if (rc != msg.hdr.len) {
        if (rc == 0) {
            ERROR("connection terminated by peer\n");
        } else {
            ERROR("send rc=%d len=%d, %s\n", rc, msg.hdr.len, strerror(errno));
        }
        return -1;
    }

    // msg has queued to be sent, return success
    return 0;
}

// -  -  -  -  -  -   SERVER - GENERATE_MSG_STATUS - -  -  -  -  -  -  -  - 

static struct msg_status_s * generate_msg_status(void)
{
    mc_status_t *mcs = mc_get_status();
    int id;
    double val;

    static struct msg_status_s x;
    static int accel_alert_count;
    static double accel_alert_last_value;

    memset(&x, 0, sizeof(x));

    // voltage and current
    x.voltage              = mcs->voltage;
    x.electronics_current  = current_get(0);
    x.motors_current       = mcs->motors_current;
    x.total_current        = x.electronics_current + x.motors_current;

    // motors and encoders
    x.mc_state              = mcs->state;
    safe_strcpy(x.mc_state_str, MC_STATE_STR(x.mc_state));
    x.mc_debug_mode_enabled = mcs->debug_mode_enabled;
    x.mc_target_speed[0]    = mcs->target_speed[0];
    x.mc_target_speed[1]    = mcs->target_speed[1];
    x.enc_poll_intvl_us     = encoder_get_poll_intvl_us();
    for (id = 0; id < 2; id++) {
        x.enc[id].enabled  = encoder_get_enabled(id);
        x.enc[id].position = encoder_get_position(id);
        x.enc[id].speed    = encoder_get_speed(id);
        x.enc[id].errors   = encoder_get_errors(id);
    }
    if (x.mc_debug_mode_enabled) {
        // these are only valid when mc_debug_mode_enabled
        for (id = 0; id < 2; id++) {
            struct debug_mode_mtr_vars_s *y = &mcs->debug_mode_mtr_vars[id];
            x.mc[id].error_status  = y->error_status;
            x.mc[id].target_speed  = y->target_speed;
            x.mc[id].current_speed = y->current_speed;
            x.mc[id].max_accel     = y->max_accel;
            x.mc[id].max_decel     = y->max_decel;
            x.mc[id].input_voltage = y->input_voltage;
            x.mc[id].current       = y->current;
        }
    }

    // proxmity sensors
    x.prox_sig_limit      = proximity_get_sig_limit();
    x.prox_poll_intvl_us  = proximity_get_poll_intvl_us();
    for (id = 0; id < 2; id++) {
        x.prox[id].enabled = proximity_get_enabled(id);
        x.prox[id].alert   = proximity_check(id, &x.prox[id].sig);
    }

    // imu
    if (imu_check_accel_alert(&val)) {
        accel_alert_count++;
        accel_alert_last_value = val;
    }
    x.heading                = imu_get_magnetometer();
    x.accel_enabled          = imu_get_accel_enabled();
    x.accel_alert_count      = accel_alert_count;
    x.accel_alert_last_value = accel_alert_last_value;
    x.accel_alert_limit      = imu_get_accel_alert_limit();

    // environment
    x.temperature_degc = env_get_temperature_degc();
    x.pressure_pascal  = env_get_pressure_pascal();
    x.temperature_degf = env_get_temperature_degf();
    x.pressure_inhg    = env_get_pressure_inhg();

    // buttons
    for (id = 0; id < 2; id++) {
        x.button[id].pressed = button_is_pressed(id);
    }

    // oled
    if (sizeof(x.oled_strs) != sizeof(oled_strs_t)) {
        FATAL("BUG: sizeof(x.oled_strs)=%zd sizeof(oled_strs_t)=%zd\n",
              sizeof(x.oled_strs), sizeof(oled_strs_t));
    }
    memcpy(x.oled_strs, oled_get_strs(), sizeof(oled_strs_t));

    return &x;
}

// -  -  -  -  -  -   SERVER - PROCESSS RECVD NSGS    -  -  -  -  -  -  -  - 

static void process_received_msg(msg_t *msg)
{
    //INFO("RECVD magic=0x%x  len=%d  id=%d\n", msg->hdr.magic, msg->hdr.len, msg->hdr.id);

    switch (msg->hdr.id) {
    case MSG_ID_DRIVE_EMER_STOP:
        drive_emer_stop();
        break;
    case MSG_ID_DRIVE_PROC:
        drive_run(&msg->drive_proc);
        break;
    case MSG_ID_MC_DEBUG_CTL:
        mc_debug_mode(msg->mc_debug_ctl.enable);
        break;
    case MSG_ID_LOG_MARK:
        INFO("------------------------------------------------\n");
        break;
    default:
        ERROR("received invalid msg id %d\n", msg->hdr.id);
        break;
    }
}
