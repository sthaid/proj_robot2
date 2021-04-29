#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
//#include <signal.h>
//#include <curses.h>
//#include <math.h>
//#include <sys/types.h>    // xxx review
//#include <sys/socket.h>

#include <misc.h>

#include <body_network_intfc.h>

//
// defines
//

#define NODE "robot-body"

//
// variables
//

static int                 sfd;
static struct msg_status_s body_status;

//
// prototypes
//

static void initialize(void);
static void *msg_receive_thread(void *cx);
static void send_msg(int id, void *data, int data_len);

// -----------------  MAIN & INITIALIZE  -----------------------------------------

int main(int argc, char **argv)
{
     initialize();
    
    printf("sending MSG_DRIVE_PROC\n");
    send_msg(MSG_ID_DRIVE_PROC, NULL, 0);

    while (true) {
        printf("VOLTAGE: %0.1f\n", body_status.voltage);
        sleep(1);
    }

    // XXX curses
    // XXX can printf after curses exits
}

static void initialize(void)
{
    static struct sockaddr_in  sockaddr;
    char s[100];
    pthread_t tid;

    // set line buffered stdout
    setlinebuf(stdout);

    // get sockaddr for body pgm
    if (getsockaddr(NODE, PORT, &sockaddr) < 0) {
        printf("ERROR: failed to get address of %s\n", NODE);
        exit(1);
    }
    printf("sockaddr = %s\n", sock_addr_to_str(s, sizeof(s), (struct sockaddr *)&sockaddr));

    // connect to body pgm
    sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (connect(sfd, (struct sockaddr*)&sockaddr, sizeof(sockaddr)) < 0) {
        printf("ERROR: failed connect to %s, %s\n",
              sock_addr_to_str(s, sizeof(s), (struct sockaddr *)&sockaddr),
              strerror(errno));
        exit(1);
    }

    // create thread to receive and process msgs from body pgm
    pthread_create(&tid, NULL, msg_receive_thread, NULL);
}

// -----------------  MSG RECIEVE THREAD  ----------------------------------------

static void *msg_receive_thread(void *cx)
{
    msg_t msg;
    int   rc;
    int   msglen;

    while (true) {
        // receive the msg.hdr
        rc = recv(sfd, &msg, sizeof(struct msg_hdr_s), MSG_WAITALL);
        if (rc != sizeof(struct msg_hdr_s)) {
            printf("ERROR: recv msg hdr rc=%d, %s\n", rc, strerror(errno));
            exit(1);  // XXX todo, review exits for interaction with curses
        }

        // validate the msg.hdr
        msglen = msg.hdr.len;  //xxx also check magic

        // receive the remainder of the msg
        rc = recv(sfd, (void*)&msg+sizeof(struct msg_hdr_s), msglen-sizeof(struct msg_hdr_s), MSG_WAITALL);
        if (rc != msglen-sizeof(struct msg_hdr_s)) {
            printf("ERROR: recv msg rc=%d, %s\n", rc, strerror(errno));
            exit(1);
        }

        // process the msg
        //printf("rcvd msg id %d\n", msg.hdr.id);
        switch (msg.hdr.id) {
        case MSG_ID_STATUS:
            body_status = msg.status;
            break;
        default:
            //xxx
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

    msg.hdr.magic = MSG_MAGIC;
    msg.hdr.len   = sizeof(struct msg_hdr_s) + data_len;
    msg.hdr.id    = id;

    // validate data_len

    if (data) {
        memcpy((void*)&msg+sizeof(struct msg_hdr_s), data, data_len);
    }

    rc = send(sfd, &msg, msg.hdr.len, MSG_NOSIGNAL);   
    if (rc != msg.hdr.len) {
        printf("ERROR: send msg rc=%d, %s\n", rc, strerror(errno));
        exit(1);
    }
}
