#include <common.h>

//
// defines
//

//#define NODE "192.168.0.11"    // direct connect
#define NODE "robot-body"        // wifi

#define MUTEX_LOCK do { pthread_mutex_lock(&mutex); } while (0)
#define MUTEX_UNLOCK do { pthread_mutex_unlock(&mutex); } while (0)

#define SEND_MSG(_msg,_succ) \
    do { \
        if (conn_sfd != -1) { \
            (_succ) = (send(conn_sfd, _msg, sizeof(msg_t), MSG_NOSIGNAL) == sizeof(msg_t)); \
        } else { \
            (_succ) = false; \
        } \
    } while (0)

#define T2S_PLAY_INTVL(us, fmt, args...) \
    do { \
        static uint64_t last; \
        uint64_t now = microsec_timer(); \
        if (now - last > (us)) { \
            t2s_play(fmt, ## args); \
            last = now; \
        } \
    } while (0)


#define GPIO_BODY_POWER  12

#define MILLION 1000000

//
// variables
//

static pthread_mutex_t              mutex;

static int                          conn_sfd = -1;
static bool                         power_is_on;
static struct msg_status_s          status;
static struct drive_proc_complete_s drive_proc_complete;
static bool                         body_emer_stop_called;

static uint64_t                     status_time;
static uint64_t                     power_on_time;
static uint64_t                     last_drive_time;

//
// prototypes
//

static void exit_handler(void);

static void *connect_and_recv_thread(void *cx);
static int connect_to_body(void);
static void disconnect_from_body(void);
static int recv_msg(msg_t *msg);
static void process_recvd_msg(msg_t *msg);

static void *monitor_thread(void *cx);

// -----------------  INIT & EXIT_HANDLER  -----------------------

void body_init(void)
{
    pthread_t tid;
    pthread_mutexattr_t mutex_attr;

    pinMode(GPIO_BODY_POWER, OUTPUT);

    pthread_mutexattr_init(&mutex_attr);
    pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&mutex, &mutex_attr);
    pthread_mutexattr_destroy(&mutex_attr);

    pthread_create(&tid, NULL, connect_and_recv_thread, NULL);
    pthread_create(&tid, NULL, monitor_thread, NULL);

    atexit(exit_handler);
}

static void exit_handler(void)
{
    // nothing needed
}

// -----------------  API  ---------------------------------------

int body_drive_cmd(int proc_id, int arg0, int arg1, int arg2, int arg3)
{
    static int unique_id;
    msg_t msg;
    bool succ;

    // xxx
    return 0;

    // acquire mutex
    MUTEX_LOCK;

    // if not connected to body return error
    if (conn_sfd == -1) {
        MUTEX_UNLOCK;
        t2s_play("brain is not connected to body");
        return -1;
    }

    // clear body_emer_stop_called flag; this flag will be
    // set if body_emer_stop is called while processing this drive command
    body_emer_stop_called = false;

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
        MUTEX_UNLOCK;
        t2s_play("failed to send message to body");
        return -1;
    }

    // keep track of the last body drive time; so that body can be powered off
    // if the body has not been driven in some time
    last_drive_time = microsec_timer();

    // release mutex
    MUTEX_UNLOCK;

    // wait for response to be received or abnormal completion
    while (drive_proc_complete.unique_id != unique_id && 
           conn_sfd != -1 &&
           body_emer_stop_called == false)
    {
        usleep(10*MS);
    }

    // reached here because one of these:
    // - received a response from body
    // - lost connection to body
    // - body_emer_stop has been called

    // handle the completion possibilities
    if (drive_proc_complete.unique_id == unique_id) {
        if (drive_proc_complete.succ) {
            return 0;
        } else {
            t2s_play("%s", drive_proc_complete.failure_reason);
            return -1;
        }
    } else if (conn_sfd == -1) {
        t2s_play("lost connection to body");
        return -1;
    } else {
        // must be body_emer_stop was called ...
        //
        // there should be a drive_proc_complete received as a result of the emer stop;
        // give one second to receive it
        int cnt = 0;
        while (drive_proc_complete.unique_id != unique_id && cnt++ < 100) {
            usleep(10*MS);
        }

        // if the drive_proc_complete was received then return completion status
        // from it, otherwise return error that the emergency stop acknowledgement was
        // not received
        if (drive_proc_complete.unique_id == unique_id) {
            if (drive_proc_complete.succ) {
                return 0;
            } else {
                t2s_play(drive_proc_complete.failure_reason);
                return -1;
            }
        } else {
            t2s_play("did not receive emergency stop acknowledgement from body");
            return -1;
        }
    }

    // should not get here
    assert(0);
}

void body_emer_stop(void)
{
    msg_t msg;
    bool succ __attribute__((unused));

    MUTEX_LOCK;

    body_emer_stop_called = true;

    msg.id = MSG_ID_DRIVE_EMER_STOP;
    SEND_MSG(&msg, succ);

    MUTEX_UNLOCK;
}

void body_power_on(void)
{
    MUTEX_LOCK;

    power_on_time = microsec_timer();
    last_drive_time = microsec_timer();
    status_time = microsec_timer();
    power_is_on = true;
    digitalWrite(GPIO_BODY_POWER, 0);

    MUTEX_UNLOCK;

    t2s_play("body power is on");
}

void body_power_off(void)
{
    MUTEX_LOCK;

    power_on_time = 0;
    last_drive_time = 0;
    status_time = 0;
    power_is_on = false;
    digitalWrite(GPIO_BODY_POWER, 1);

    MUTEX_UNLOCK;

    t2s_play("body power is off");
}

void body_status_report(void)
{
    if (!power_is_on) {
        t2s_play("Bbody is off.");
    } else if (conn_sfd == -1) {
        t2s_play("Brain is not connected to body.");
    } else if (microsec_timer() - status_time > 5*MILLION) {
        t2s_play("Status message has not been received from the body.");
    } else {
        t2s_play("Voltage is"); t2s_play_nodb("%0.2f volts", status.voltage);
        t2s_play("Current is"); t2s_play_nodb("%0.0f milliamps", 1000*status.total_current);
        t2s_play("Magnetic heading is"); t2s_play_nodb("%0.0f degrees", status.mag_heading);
    }
}

void body_weather_report(void)
{
    if (!power_is_on) {
        t2s_play("Body is off.");
    } else if (conn_sfd == -1) {
        t2s_play("Brain is not connected to body.");
    } else if (microsec_timer() - status_time > 5*MILLION) {
        t2s_play("Status message has not been received from the body.");
    } else {
        t2s_play("Temperature is %0.0f degrees", status.temperature_degf);
        t2s_play("Pressure is %0.1f inches of mercury", status.pressure_inhg);
    }
}

// -----------------  CONNECT AND RECEIVE THREAD  ----------------

static void *connect_and_recv_thread(void *cx)
{
    msg_t msg;

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
        ERROR_INTVL(60*SECONDS, "failed connect to %s, %s\n",
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
    MUTEX_LOCK;
    conn_sfd = sfd;
    status_time = microsec_timer();
    MUTEX_UNLOCK;

    // issue connected msg
    t2s_play("brain is connected to body");
    return 0;
}

static void disconnect_from_body(void)
{
    // close socket and set conn_sfd to -1
    MUTEX_LOCK;
    if (conn_sfd != -1) {
        close(conn_sfd);
        conn_sfd = -1;
        status_time = 0;
    }
    MUTEX_UNLOCK;

    // issue disconected msg
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
            usleep(10*MS);
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
        status_time = microsec_timer();
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

// -----------------  MONITOR THREAD  ----------------------------

static void *monitor_thread(void *cx)
{
    #define SECS(us)  ((int)((time_now - (us)) / MILLION))

    uint64_t time_now;

    while (true) {
        MUTEX_LOCK;

        time_now = microsec_timer();

        if (power_is_on) do {
            // if body power has been on for 60 seconds, but connection not established 
            // then issue warning
            if (conn_sfd == -1 && SECS(power_on_time) > 60) {
                //xxx T2S_PLAY_INTVL(60*MILLION, "Brain is not connected to body.");
                break;
            }

            // if body has not been used in 10 minutes then disconnect from body and power off body
            if (SECS(last_drive_time) > 600) {
                t2s_play("Body has not been used for 10 minutes, powering off body.");
                disconnect_from_body();
                body_power_off();
                break;
            }

            // if connection to body has not been established then continue
            if (conn_sfd == -1) {
                break;
            }

            if (SECS(status_time) > 5) {
                T2S_PLAY_INTVL(5*MILLION, 
                    "Status message has not been received from the body in %d seconds\n",
                    SECS(status_time));
                break;
            }

            // --- the following code issues reports based on the latest status ---

            // check status.voltage
            if (status.voltage < 11.5) {
                T2S_PLAY_INTVL(60*MILLION, 
                    "Voltage %0.1f is critically low, shutdown and recharge now.",
                    status.voltage);
            } else if (status.voltage < 12) {
                T2S_PLAY_INTVL(600*MILLION, 
                    "Voltage %0.1f is low, recharge soon.",
                    status.voltage);
            }
        } while (0);

        MUTEX_UNLOCK;

        sleep(1);
    }

    return NULL;
}
