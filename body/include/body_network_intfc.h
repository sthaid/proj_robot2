#ifndef __BODY_NETWORK_INTFC_H__
#define __BODY_NETWORK_INTFC_H__

#ifdef __cplusplus
extern "C" {
#endif

#define PORT      9777
#define MSG_MAGIC 0x12345678

// msgs sent from client to body
#define MSG_ID_DRIVE_CAL     1
#define MSG_ID_DRIVE_PROC    2
#define MSG_ID_MC_DEBUG_CTL  3

// msgs sent from body to client
#define MSG_ID_STATUS        21

typedef struct {
    struct msg_hdr_s {
        int magic;
        int len;
        int id;
    } hdr;
    union {
        struct {
            int no_args;
        } drive_cal;
        struct {
            int proc_id;
        } drive_proc;
        struct {
            int enable;
        } mc_debug_ctl;
        struct msg_status_s {
            double voltage;
            double total_current;
            double electronics_current;
            double motors_current;
        } status;
    };
} msg_t;

#ifdef __cplusplus
}
#endif

#endif

