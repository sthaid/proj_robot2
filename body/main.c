#include "common.h"

//
// defines
//

// dest must be a char array, and not a char *
#define safe_strncpy(dest, src) \
    do { \
        strncpy(dest, src, sizeof(dest)-1); \
        (dest)[sizeof(dest)-1] = '\0'; \
    } while (0)

#define MUTEX_LOCK do { pthread_mutex_lock(&mutex); } while (0)
#define MUTEX_UNLOCK do { pthread_mutex_unlock(&mutex); } while (0)

//
// typedefs
//

//
// variables
//

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static int sockfd = -1;
static bool sigint, sigterm;

//
// prototypes
//

static void initialize(void);
static void sig_hndlr(int signum);
static void logmsg_cb(char *str);

static void server(void);
static int recv_msg(msg_t *msg);
static void process_received_msg(msg_t *msg);

static void *send_status_msg_thread(void *cx);
static void generate_status_msg(msg_t *msg);
static void send_logmsg(char *str);
static void send_msg(msg_t *msg);

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
    pthread_t tid;

    #define CALL(routine,args) \
        do { \
            int rc = routine args; \
            if (rc < 0) { \
                fprintf(stderr, "FATAL: %s failed\n", #routine); \
                exit(1); \
            } \
        } while (0)

    // register SIGTERM and SIGINT
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = sig_hndlr;
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGTERM, &act, NULL);

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

    // create send_status_msg_thread
    pthread_create(&tid, NULL, send_status_msg_thread, NULL);
}

static void sig_hndlr(int signum)
{
    if (signum == SIGINT) sigint = true;
    if (signum == SIGTERM) sigterm = true;
}

static void logmsg_cb(char *str)
{
    send_logmsg(str);
}

// -----------------  ACCEPT CONN, RECV & PROCESS MSGS  --------------------

static void server(void)
{
    struct sockaddr_in server_address;
    int                listen_sockfd;
    int                ret;
    int                optval;
    char               client[200];

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

    // accept connection
accept_connection:
    while (true) {
        socklen_t len;
        struct sockaddr_in address;

        len = sizeof(address);
        sockfd = accept(listen_sockfd, (struct sockaddr *) &address, &len);
        if (sockfd == -1) {
            if (sigint || sigterm) return;
            ERROR("accept, %s\n", strerror(errno));
            continue;
        }
        INFO("accepted connection from %s\n", sock_addr_to_str(client,sizeof(client),(struct sockaddr *)&address));
        break;
    }

    // recv and process messages
    while (true) {
        msg_t msg;

        if (recv_msg(&msg) < 0) {
            break;
        }

        process_received_msg(&msg);
    }

    // print disconnected and close sockfd
    INFO("disconnecting from %s\n", client);
    MUTEX_LOCK;
    close(sockfd);
    sockfd = -1;
    MUTEX_UNLOCK;

    // if program is terminating then return otherwise accept connection
    if (sigint || sigterm) {
        return;
    } else {
        goto accept_connection;
    }
}

static int recv_msg(msg_t *msg)
{
    int rc;

    // receive the msg
    rc = recv(sockfd, msg, sizeof(msg_t), MSG_WAITALL);
    if (rc != sizeof(msg_t)) {
        if (rc == 0) {
            ERROR("connection closed by peer\n");
        } else {
            ERROR("recv msg rc=%d, %s\n", rc, strerror(errno));
        }
        return -1;
    }

    // validate the msg->id
    if (msg->id != MSG_ID_DRIVE_EMER_STOP &&
        msg->id != MSG_ID_DRIVE_PROC &&
        msg->id != MSG_ID_MC_DEBUG_CTL &&
        msg->id != MSG_ID_LOG_MARK)
    {
        ERROR("invalid msg id 0x%x\n", msg->id);
        return -1;
    }

    // success
    return 0;
}

static void process_received_msg(msg_t *msg)
{
    switch (msg->id) {
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
        FATAL("received invalid msg id %d\n", msg->id);
        break;
    }
}

// -----------------  SEND STATUS MSG THREAD  ------------------------------

static void *send_status_msg_thread(void *cx)
{
    msg_t msg;

    while (true) {
        if (sockfd != -1) {
            generate_status_msg(&msg);
            send_msg(&msg);
        }

        usleep(500000);
    }

    return NULL;
}

static void generate_status_msg(msg_t *msg)
{
    mc_status_t *mcs = mc_get_status();
    int id;
    double val;
    struct msg_status_s *x = &msg->status;

    static int accel_alert_count;
    static double accel_alert_last_value;

    // set msg id
    memset(msg, 0, sizeof(msg_t));
    msg->id = MSG_ID_STATUS;

    // voltage and current
    x->voltage              = mcs->voltage;
    x->electronics_current  = current_get(0);
    x->motors_current       = mcs->motors_current;
    x->total_current        = x->electronics_current + x->motors_current;

    // motors and encoders
    x->mc_state              = mcs->state;
    safe_strncpy(x->mc_state_str, MC_STATE_STR(x->mc_state));
    x->mc_debug_mode_enabled = mcs->debug_mode_enabled;
    x->mc_target_speed[0]    = mcs->target_speed[0];
    x->mc_target_speed[1]    = mcs->target_speed[1];
    x->enc_poll_intvl_us     = encoder_get_poll_intvl_us();
    for (id = 0; id < 2; id++) {
        x->enc[id].enabled  = encoder_get_enabled(id);
        x->enc[id].position = encoder_get_count(id);
        x->enc[id].speed    = encoder_get_speed(id);
        x->enc[id].errors   = encoder_get_errors(id);
    }
    if (x->mc_debug_mode_enabled) {
        // these are only valid when mc_debug_mode_enabled
        for (id = 0; id < 2; id++) {
            struct debug_mode_mtr_vars_s *y = &mcs->debug_mode_mtr_vars[id];
            x->mc[id].error_status  = y->error_status;
            x->mc[id].target_speed  = y->target_speed;
            x->mc[id].current_speed = y->current_speed;
            x->mc[id].max_accel     = y->max_accel;
            x->mc[id].max_decel     = y->max_decel;
            x->mc[id].input_voltage = y->input_voltage;
            x->mc[id].current       = y->current;
        }
    }

    // proxmity sensors
    x->prox_sig_limit      = proximity_get_sig_limit();
    x->prox_poll_intvl_us  = proximity_get_poll_intvl_us();
    for (id = 0; id < 2; id++) {
        x->prox[id].enabled = proximity_get_enabled(id);
        x->prox[id].alert   = proximity_check(id, &x->prox[id].sig);
    }

    // imu
    if (imu_check_accel_alert(&val)) {
        accel_alert_count++;
        accel_alert_last_value = val;
    }
    x->mag_heading            = imu_get_magnetometer();
    x->rotation               = imu_get_rotation();
    x->accel_rot_enabled      = imu_get_accel_rot_ctrl();
    x->accel_alert_count      = accel_alert_count;
    x->accel_alert_last_value = accel_alert_last_value;
    x->accel_alert_limit      = imu_get_accel_alert_limit();

    // environment
    x->temperature_degc = env_get_temperature_degc();
    x->pressure_pascal  = env_get_pressure_pascal();
    x->temperature_degf = env_get_temperature_degf();
    x->pressure_inhg    = env_get_pressure_inhg();

    // buttons
    for (id = 0; id < 2; id++) {
        x->button[id].pressed = button_is_pressed(id);
    }

    // oled
    if (sizeof(x->oled_strs) != sizeof(oled_strs_t)) {
        FATAL("BUG: sizeof(x->oled_strs)=%zd sizeof(oled_strs_t)=%zd\n",
              sizeof(x->oled_strs), sizeof(oled_strs_t));
    }
    memcpy(x->oled_strs, oled_get_strs(), sizeof(oled_strs_t));
}

// -----------------  SEND MSG PROCS  --------------------------------------

static void send_logmsg(char *str)
{
    msg_t msg;

    msg.id = MSG_ID_LOGMSG;
    safe_strncpy(msg.logmsg.str, str);
    send_msg(&msg);
}

static void send_msg(msg_t *msg)
{
    int rc;

    // if client not connected then return
    MUTEX_LOCK;
    if (sockfd == -1) {
        MUTEX_UNLOCK;
        return;
    }

    // send the msg
    rc = send(sockfd, msg, sizeof(msg_t), MSG_NOSIGNAL);
    if (rc != sizeof(msg_t)) {
        if (rc == 0) {
            ERROR("connection terminated by peer\n");
        } else {
            ERROR("send rc=%d sizeof(msg_t)=%zd, %s\n", rc, sizeof(msg_t), strerror(errno));
        }
        MUTEX_UNLOCK;
        return;
    }

    // unlock mutex
    MUTEX_UNLOCK;
}
