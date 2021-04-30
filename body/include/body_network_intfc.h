#ifndef __BODY_NETWORK_INTFC_H__
#define __BODY_NETWORK_INTFC_H__

#ifdef __cplusplus
extern "C" {
#endif

#define PORT                9777
#define MSG_MAGIC           0x12345678
#define MAX_LOGMSG_STR_SIZE 100

// msgs sent from client to body
#define MSG_ID_DRIVE_CAL     1
#define MSG_ID_DRIVE_PROC    2
#define MSG_ID_MC_DEBUG_CTL  3

// msgs sent from body to client
#define MSG_ID_STATUS        21
#define MSG_ID_LOGMSG        22

typedef struct {
    struct msg_hdr_s {
        int magic;
        int len;
        int id;
        int pad;
    } hdr;
    union {
        struct msg_drive_cal_s {
            int no_args;
        } drive_cal;
        struct msg_drive_proc_s {
            int proc_id;
        } drive_proc;
        struct msg_mc_debug_ctl_s {
            int enable;
        } mc_debug_ctl;
        struct msg_status_s {
            double voltage;
            double total_current;
            double electronics_current;
            double motors_current;
        } status;
        struct msg_logmsg_s {
            char str[MAX_LOGMSG_STR_SIZE];
        } logmsg;
    };
} msg_t;

#ifdef __cplusplus
}
#endif

#endif

