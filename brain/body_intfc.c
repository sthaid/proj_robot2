#include <common.h>

#include "../body/include/body_network_intfc.h"

// defines
#define NODE "robot-body"

// variables
static bool                powered_on;
static int                 conn_sfd = -1;
static struct msg_status_s status;

// prototypes
static void *connect_and_recv_thread(void *cx);
static int connect_to_body(void);
static void disconnect_from_body(void);
static int recv_msg(msg_t *msg);

// ---------------------------------------------------------------

void body_intfc_init(void)
{
    pthread_t tid;

    // xxx get body_pwr_is_on from db
    powered_on = true;

    pthread_create(&tid, NULL, connect_and_recv_thread, NULL);

    // xxx atexit
}

// ---------------------------------------------------------------

static void *connect_and_recv_thread(void *cx)
{
    msg_t msg;

    while (true) {
        // if body is not on then delay and contine
        if (!powered_on) {
            sleep(1);
            continue;
        }

        // if not connected then establish connection
        if (conn_sfd == -1) {
            if (connect_to_body() < 0) {
                sleep(1);
                continue;
            }
        }

        // recv msg
        if (recv_msg(&msg) < 0) {
            disconnect_from_body();
            sleep(1);
            continue;
        }

        // process the msg
        switch (msg.hdr.id) {
        case MSG_ID_STATUS:
            status = msg.status;
            break;
        case MSG_ID_LOGMSG:
            INFO("BODY: %s\n", msg.logmsg.str);
            break;
        default:
            disconnect_from_body();
            sleep(1);
            continue;
        }
    }

    return NULL;
}

static int connect_to_body(void)
{
    int sfd, rc;
    static struct sockaddr_in  sockaddr;
    char s[100];
    struct timeval tv = {3,0};

    // get sockaddr for body pgm
    if (getsockaddr(NODE, PORT, &sockaddr) < 0) {
        ERROR("failed to get address of %s", NODE);
        return -1;
    }

    // connect to body pgm
    sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (connect(sfd, (struct sockaddr*)&sockaddr, sizeof(sockaddr)) < 0) {
        ERROR("failed connect to %s, %s",
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
    conn_sfd = sfd;
    return 0;
}

static void disconnect_from_body(void)
{
    int sfd = conn_sfd;

    // if already closed then return
    if (sfd == -1) return;

    // set conn_sfd to -1, indicating not connected; and
    // close the socket
    conn_sfd = -1;
    close(sfd);
}

static int recv_msg(msg_t *msg)
{
    int rc;
    int sfd = conn_sfd;

    // if not connected return error
    if (sfd == -1) {
        return -1;
    }

    // receive the msg->hdr
    rc = recv(sfd, &msg, sizeof(struct msg_hdr_s), MSG_WAITALL);
    if (rc != sizeof(struct msg_hdr_s)) {
        if (rc == 0) {
            ERROR("connection closed by peer");
        } else {
            ERROR("recv msg hdr rc=%d, %s", rc, strerror(errno));
        }
        return -1;
    }

    // validate the msg->hdr
    if (msg->hdr.magic != MSG_MAGIC ||
        msg->hdr.len < sizeof(struct msg_hdr_s) ||
        msg->hdr.len > sizeof(msg_t))
    {
        ERROR("invalid msg magic 0x%x or len %d", msg->hdr.magic, msg->hdr.len);
        return -1;
    }

    // receive the remainder of the msg
    rc = recv(sfd, (void*)&msg+sizeof(struct msg_hdr_s), msg->hdr.len-sizeof(struct msg_hdr_s), MSG_WAITALL);
    if (rc != msg->hdr.len-sizeof(struct msg_hdr_s)) {
        if (rc == 0) {
            ERROR("connection closed by peer");
        } else {
            ERROR("recv msg rc=%d, %s", rc, strerror(errno));
        }
        return -1;
    }

    // success
    return 0;
}

// ---------------------------------------------------------------

int body_intfc_send_msg(int id, void *data, int data_len)
{
    msg_t msg;
    int rc;
    int sfd = conn_sfd;

    // if not connected return error
    if (sfd == -1) {
        return -1;
    }

    // validate data_len
    if ((data == NULL && data_len != 0) ||
        (data != NULL && (data_len <= 0 || data_len > sizeof(msg_t)-sizeof(struct msg_hdr_s))))
    {
        ERROR("data=%p data_len=%d", data, data_len);
        return -1;        
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
            ERROR("connection closed by peer");
        } else {
            ERROR("send msg rc=%d, %s", rc, strerror(errno));
        }
        return -1;        
    }

    // msg is queued to be sent
    return 0;
}
