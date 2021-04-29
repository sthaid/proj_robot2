// xxx review use of FATAL

#include "common.h"

//
// defines
//

#define MAX_LOGMSG_STRS 100
#define LOG_FILENAME "body.log"

//
// typedefs
//

//
// variables
//

#if 0 //XXX todo
static char logmsg_strs[MAX_LOGMSG_STRS][200];
static int  logmsg_strs_tail;
static bool logmsg_monitor_thread_running;
#endif

static bool sigterm;

//
// prototypes
//

static void initialize(void);
static void sigterm_hndlr(int signum);
//static void *logmsg_monitor_thread(void *cx);

static void server(void);
static void *server_thread(void *cx);
static void process_received_msg(msg_t *msg);
static void generate_status_msg(msg_t *status_msg);

// -----------------  MAIN AND INIT ROUTINES  ------------------------------

int main(int argc, char **argv)
{
    // init
    initialize();

    // call server to accept and process connections from clients
    // xxx how does this return, what about sigterm
    // xxx add scripts to control starting and stopping, and add to /etc/rc.local
    server();

    // stop motors
    mc_disable_all();

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

#if 0
    // init logging to LOG_FILENAME
    if (logmsg_init(LOG_FILENAME) < 0) {
        fprintf(stderr, "FATAL: logmsg_init failed, %s\n", strerror(errno));
        exit(1);;
    }

    // create thread to read the tail of logmsg and copy
    // the new lines added to the logmsg to logmsgs array
    pthread_create(&tid, NULL, logmsg_monitor_thread, NULL);
    while (logmsg_monitor_thread_running == false) {
        usleep(1000);
    }
#endif

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
    act.sa_handler = sigterm_hndlr;
    sigaction(SIGTERM, &act, NULL);
}

static void sigterm_hndlr(int signum)
{
    // xxx where used, and test
    sigterm = true;
}

#if 0
// XXX a better way?
static void *logmsg_monitor_thread(void *cx)
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
    logmsg_monitor_thread_running = true;

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
#endif

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
    if (listen_sockfd == -1) {
        FATAL("socket, %s\n", strerror(errno));
    }

    // set reuseaddr
    optval = 1;
    ret = setsockopt(listen_sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, 4);
    if (ret == -1) {
        FATAL("SO_REUSEADDR, %s\n", strerror(errno));
    }

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
    ret = listen(listen_sockfd, 2);
    if (ret == -1) {
        FATAL("listen, %s\n", strerror(errno));
    }

    // init thread attributes to make thread detached
    if (pthread_attr_init(&attr) != 0) {
        FATAL("pthread_attr_init, %s\n", strerror(errno));
    }
    if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) != 0) {
        FATAL("pthread_attr_setdetachstate, %s\n", strerror(errno));
    }

    // loop, accepting connection, and create thread to service the client
    // xxx what typically sees the sigterm
    INFO("server: accepting connections\n");
    while (1) {
        int                sockfd;
        socklen_t          len;
        struct sockaddr_in address;

        // accept connection
        len = sizeof(address);
        sockfd = accept(listen_sockfd, (struct sockaddr *) &address, &len);
        if (sockfd == -1) {
            if (sigterm) {
                break;
            }
            FATAL("accept, %s\n", strerror(errno));
        }

        // create thread
        INFO("accepting connection from %s\n", 
             sock_addr_to_str(s,sizeof(s),(struct sockaddr *)&address));

        if (pthread_create(&thread, &attr, server_thread, (void*)(uintptr_t)sockfd) != 0) {
            FATAL("pthread_create server_thread, %s\n", strerror(errno));
        }
    }
}

static void * server_thread(void * cx)
{
    int      sockfd = (uintptr_t)cx;
    msg_t    recv_msg;
    msg_t    status_msg;
    uint64_t time_last_send = microsec_timer();
    uint64_t time_now;
    int      rc;
    int      recvd_len = 0;

    while (true) {
        // perform non-blocking recv of msg, this is accomplished by
        // first receiving the hdr (which includes the msg len), and then
        // receiving the rest of the msg
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
                close(sockfd);
                return NULL;
            }
            // call recv to receive the remainder of the msg
            rc = recv(sockfd, (void*)&recv_msg+recvd_len, recv_msg.hdr.len-recvd_len, MSG_DONTWAIT);
        }
        if (rc > 0) {
            recvd_len += rc;
            if (recvd_len > 8 && recvd_len == recv_msg.hdr.len) {
                process_received_msg(&recv_msg);
                memset(&recv_msg, 0, recvd_len);
                recvd_len = 0;
            }
        } else if (rc == 0) {
            // terminate connection
            INFO("connection terminated by peer\n");
            close(sockfd);
            return NULL;
        } else if (errno != EWOULDBLOCK && errno != EAGAIN) {
            // errno is unexpected, print msg and terminate connection
            ERROR("recv failed, %s\n", strerror(errno));
            close(sockfd);
            return NULL;
        }

        // if it has been 100 ms since last sending status msg then do so now
        time_now = microsec_timer();
        if (time_now - time_last_send > 100000) {
            generate_status_msg(&status_msg);
            rc = send(sockfd, &status_msg, status_msg.hdr.len, MSG_NOSIGNAL);
            if (rc != status_msg.hdr.len) {
                ERROR("send status msg failed, rc=%d len=%d, %s\n",
                      rc, status_msg.hdr.len, strerror(errno));
                close(sockfd);
                return NULL;
            }
            time_last_send = time_now;
        }

        // short sleep 
        usleep(25000);  // 25 ms
    }

    return NULL;
}

// xxx msg to abort a drive proc
// xxx possibly also when client exits
static void process_received_msg(msg_t *msg)
{
    INFO("RECVD magic=0x%x  len=%d  id=%d\n", msg->hdr.magic, msg->hdr.len, msg->hdr.id);

    switch (msg->hdr.id) {
    case MSG_ID_DRIVE_CAL:
        drive_run_cal();
        break;
    case MSG_ID_DRIVE_PROC:
        drive_run_proc(msg->drive_proc.proc_id);
        break;
    case MSG_ID_MC_DEBUG_CTL:
        mc_debug_mode(msg->mc_debug_ctl.enable);
        break;
    default:
        ERROR("received invalid msg id %d\n", msg->hdr.id);
        break;
    }
}

static void generate_status_msg(msg_t *msg)
{
    mc_status_t *mcs = mc_get_status();

    msg->hdr.magic = MSG_MAGIC;
    msg->hdr.len   = sizeof(msg_t);
    msg->hdr.id    = MSG_ID_STATUS;

    msg->status.voltage              = mcs->voltage;
    msg->status.electronics_current  = current_read_smoothed(0);
    msg->status.motors_current       = mcs->motors_current;
    msg->status.total_current        = msg->status.electronics_current + msg->status.motors_current;
}

