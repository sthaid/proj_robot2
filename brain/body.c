#include <common.h>

#include "../body/include/body_network_intfc.h"

//
// defines
//

//#define NODE "192.168.0.11"
#define NODE "robot-body"

#define MUTEX_LOCK do { pthread_mutex_lock(&mutex); } while (0)
#define MUTEX_UNLOCK do { pthread_mutex_unlock(&mutex); } while (0)

#define SEND_MSG(_msg,_succ) \
    do { \
        MUTEX_LOCK; \
        if (conn_sfd != -1) { \
            (_succ) = (send(conn_sfd, _msg, sizeof(msg_t), MSG_NOSIGNAL) == sizeof(msg_t)); \
        } else { \
            (_succ) = false; \
        } \
        MUTEX_UNLOCK; \
    } while (0)

//
// variables
//

static struct msg_status_s          status;  // xxx need to use this
static int                          conn_sfd = -1;
static bool                         power_is_on;
static struct drive_proc_complete_s drive_proc_complete;
static pthread_mutex_t              mutex = PTHREAD_MUTEX_INITIALIZER;

//
// prototypes
//

static void exit_handler(void);

static void *connect_and_recv_thread(void *cx);
static int connect_to_body(void);
static void disconnect_from_body(void);
static int recv_msg(msg_t *msg);
static void process_recvd_msg(msg_t *msg);

// -----------------  INIT & EXIT_HANDLER  -----------------------

void body_init(void)
{
    pthread_t tid;

    body_power_on();
    pthread_create(&tid, NULL, connect_and_recv_thread, NULL);
    atexit(exit_handler);
}

static void exit_handler(void)
{
    body_power_off();
}

// -----------------  API  ---------------------------------------

int body_drive_cmd(int proc_id, int arg0, int arg1, int arg2, int arg3, char *failure_reason)
{
    static int unique_id;
    msg_t msg;
    bool succ;

    // preset failure_reset to empty string
    failure_reason[0] = '\0';

    // send the body drive request
    msg.id = MSG_ID_DRIVE_PROC;
    msg.drive_proc.proc_id = proc_id;;
    msg.drive_proc.unique_id = ++unique_id;
    msg.drive_proc.arg[0] = arg0;
    msg.drive_proc.arg[1] = arg1;
    msg.drive_proc.arg[2] = arg2;
    msg.drive_proc.arg[3] = arg3;

    SEND_MSG(&msg, succ);
    if (!succ) {
        strcpy(failure_reason, "failed to send message to body");
        return -1;
    }

    // wait for response to be received
    while (drive_proc_complete.unique_id != unique_id) {
        usleep(1000);
    }

    // if body drive command failed then copy the failure_reason string to caller
    if (!drive_proc_complete.succ) {
        strcpy(failure_reason, drive_proc_complete.failure_reason);
    }

    // return 0 for success, -1 for failure
    return drive_proc_complete.succ ? 0 : -1;
}

void body_emer_stop(void)
{
    msg_t msg;
    bool succ __attribute__((unused));

    msg.id = MSG_ID_DRIVE_EMER_STOP;
    SEND_MSG(&msg, succ);
}

void body_power_on(void)
{
    // xxx set gpio
    power_is_on = true;
}

void body_power_off(void)
{
    // xxx set gpio
    power_is_on = false;
}

// -----------------  CONNECT AND RECEIVE  -----------------------

static void *connect_and_recv_thread(void *cx)
{
    msg_t msg;

    sleep(1);

    while (true) {
        // if body is not on then delay and contine
        if (power_is_on == false) {
            sleep(1);
            continue;
        }

        // if not connected then establish connection
        if (conn_sfd == -1) {
            if (connect_to_body() < 0) {
                sleep(5);
                continue;
            }
            assert(conn_sfd != -1);
        }

        // recv msg
        if (recv_msg(&msg) < 0) {
            disconnect_from_body();
            sleep(1);
            continue;
        }

        // process the received msg
        process_recvd_msg(&msg);
    }

    return NULL;
}

static int connect_to_body(void)
{
    int sfd, rc;
    static struct sockaddr_in  sockaddr;
    char s[100];
    struct timeval tv = {3,0};

    assert(conn_sfd == -1);

    // get sockaddr for body pgm
    if (getsockaddr(NODE, PORT, &sockaddr) < 0) {
        //ERROR("failed to get address of %s\n", NODE);
        return -1;
    }

    // connect to body pgm
    sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (connect(sfd, (struct sockaddr*)&sockaddr, sizeof(sockaddr)) < 0) {
        ERROR_INTVL(60000, "failed connect to %s, %s\n",
                    sock_addr_to_str(s, sizeof(s), (struct sockaddr *)&sockaddr),
                    strerror(errno));
        close(sfd);
        return -1;
    }

    // set 3 second recv timeout
    rc = setsockopt(sfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    if (rc < 0) {
        ERROR("failed to set SO_RCVTIMEO, %s\n", strerror(errno));
        close(sfd);
        return -1;
    }

    // set global variable conn_sfd, indiating connection is established,
    // and return success
    INFO("connected to body\n");
    t2s_play("connected to body");
    conn_sfd = sfd;
    return 0;
}

static void disconnect_from_body(void)
{
    // close socket and set conn_sfd to -1
    MUTEX_LOCK;
    if (conn_sfd != -1) {
        close(conn_sfd);
        conn_sfd = -1;
    }
    MUTEX_UNLOCK;

    // print disconected msg
    INFO("disconnected from body\n");
    t2s_play("disconnected from body");
}

static int recv_msg(msg_t *msg)
{
    int rc;

    assert(conn_sfd != -1);

    // receive the msg
    rc = recv(conn_sfd, msg, sizeof(msg_t), MSG_WAITALL);
    if (rc != sizeof(msg_t)) {
        if (rc == 0) {
            ERROR("connection closed by peer\n");
        } else {
            ERROR("recv msg rc=%d, %s\n", rc, strerror(errno));
        }
        return -1;
    }

    // validate the msg->id
    if (msg->id != MSG_ID_STATUS &&
        msg->id != MSG_ID_LOGMSG &&
        msg->id != MSG_ID_DRIVE_PROC_COMPLETE)
    {
        ERROR("invalid msg id 0x%x\n", msg->id);
        return -1;
    }

    // success
    return 0;
}

static void process_recvd_msg(msg_t *msg)
{
    switch (msg->id) {
    case MSG_ID_STATUS:
        //INFO("BODY: got msg status\n");
        status = msg->status;
        break;
    case MSG_ID_LOGMSG:
        INFO("BODY: %s\n", msg->logmsg.str);
        break;
    case MSG_ID_DRIVE_PROC_COMPLETE:
        drive_proc_complete = msg->drive_proc_complete;
        break;
    default:
        FATAL("XXX");
        break;
    }
}
