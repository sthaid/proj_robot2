// xxx turn off body if not being used for 10 minutes

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

#define GPIO_BODY_POWER  12

//
// variables
//

static struct msg_status_s          status;
static uint64_t                     status_time_us;
static int                          conn_sfd = -1;
static bool                         power_is_on;
static uint64_t                     power_state_change_time_us;
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

    pinMode(GPIO_BODY_POWER, OUTPUT);
    //xxx pthread_create(&tid, NULL, connect_and_recv_thread, NULL);
    atexit(exit_handler);
}

static void exit_handler(void)
{
    // xxx nothing?
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
    digitalWrite(GPIO_BODY_POWER, 0);
    power_is_on = true;
    power_state_change_time_us = microsec_timer();
    t2s_play("body power is on");
}

void body_power_off(void)
{
    digitalWrite(GPIO_BODY_POWER, 1);
    power_is_on = false;
    power_state_change_time_us = microsec_timer();
    t2s_play("body power is off");
}

void body_status_report(void)
{
    if (!power_is_on) {
        t2s_play("Bbody is off.");
    } else if (conn_sfd == -1) {
        t2s_play("Brain is not connected to body.");
    } else if (microsec_timer() - status_time_us > 3000000) {
        t2s_play("Status message has not been received from the body.");
    } else {
        t2s_play("Voltage = %0.2f volts", status.voltage);
        t2s_play("Current = %0.0f milliamps", 1000*status.total_current);
        t2s_play("Magnetic heading = %0.0f degrees", status.mag_heading);
    }
}

void body_weather_report(void)
{
    if (!power_is_on) {
        t2s_play("Bbody is off.");
    } else if (conn_sfd == -1) {
        t2s_play("Brain is not connected to body.");
    } else if (microsec_timer() - status_time_us > 3000000) {
        t2s_play("Status message has not been received from the body.");
    } else {
        t2s_play("Temperature = %0.0f degrees", status.temperature_degf);
        t2s_play("Pressure = %0.2f inches of mercury", status.pressure_inhg);
    }
}

// -----------------  CONNECT AND RECEIVE  -----------------------

static void *connect_and_recv_thread(void *cx)
{
    msg_t msg;
    bool notified = false;

    sleep(3);
    body_power_on();

    while (true) {
        // if body is not on then delay and contine
        if (power_is_on == false) {
            sleep(1);
            continue;
        }

        // if not connected then establish connection
        if (conn_sfd == -1) {
            if (connect_to_body() < 0) {
                // if the body is powered up but hasn't connected then notify of such
                if ((microsec_timer() - power_state_change_time_us > 60000000) && !notified) {
                    t2s_play("body is on but not connected to brain");
                    notified = true;
                }

                // sleep 5 secs and continue
                sleep(5);
                continue;
            }
            notified = false;
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
    t2s_play("brain is connected to body");
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
    t2s_play("brain is disconnected from body");
}

static int recv_msg(msg_t *msg)
{
    int rc;
    int eagain_count = 0;

    assert(conn_sfd != -1);

    // receive the msg
try_again:
    rc = recv(conn_sfd, msg, sizeof(msg_t), MSG_WAITALL);
    if (rc != sizeof(msg_t)) {
        if (rc == -1 && errno == EAGAIN && eagain_count++ < 20) {
            WARN("EAGAIN, delay and try again\n");
            usleep(100000);
            goto try_again;
        } else if (rc == 0) {
            ERROR("connection closed by peer\n");
            return -1;
        } else {
            ERROR("recv msg rc=%d, %s\n", rc, strerror(errno));
            return -1;
        }
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
        status = msg->status;
        status_time_us = microsec_timer();
        break;
    case MSG_ID_LOGMSG:
        INFO("BODY: %s\n", msg->logmsg.str);
        break;
    case MSG_ID_DRIVE_PROC_COMPLETE:
        drive_proc_complete = msg->drive_proc_complete;
        break;
    default:
        assert(0);
        break;
    }
}
