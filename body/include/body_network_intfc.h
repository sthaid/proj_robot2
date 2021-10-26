#ifndef __BODY_NETWORK_INTFC_H__
#define __BODY_NETWORK_INTFC_H__

#ifdef __cplusplus
extern "C" {
#endif

// general defines
#define PORT                  9777
#define MAX_LOGMSG_STR_SIZE   100
#define MAX_OLED_STR          5
#define MAX_OLED_STR_SIZE     10
#define MAX_DRIVE_PROC_COMPLETE_REASON_STR_SIZE 100

// msgs sent from client to body
#define MSG_ID_DRIVE_EMER_STOP      0x1001
#define MSG_ID_DRIVE_PROC           0x1002
#define MSG_ID_MC_DEBUG_CTL         0x1003
#define MSG_ID_LOG_MARK             0x1004

// msgs sent from body to client
#define MSG_ID_STATUS               0x2001
#define MSG_ID_LOGMSG               0x2002
#define MSG_ID_DRIVE_PROC_COMPLETE  0x2003

typedef struct {
    int id;
    union {
        struct msg_drive_proc_s {
            int proc_id;
            int unique_id;
	    double arg[8];
        } drive_proc;
        struct {
            int unique_id;
            char reason[MAX_DRIVE_PROC_COMPLETE_REASON_STR_SIZE];
        } drive_proc_complete;
        struct msg_mc_debug_ctl_s {
            int enable;
        } mc_debug_ctl;
        struct msg_status_s {
            // voltage and current
            double voltage;
            double total_current;
            double electronics_current;
            double motors_current;
            // motors and encoders
            int mc_state;
            char mc_state_str[16];
            int mc_debug_mode_enabled;
            int mc_target_speed[2];
            int enc_poll_intvl_us;
            struct {
                int enabled;
                int position;
                int speed;
                int errors;
            } enc[2];
            struct {  // these are only valid when mc_debug_enabled==true
                int error_status;
                int target_speed;
                int current_speed;
                int max_accel;
                int max_decel;
                double input_voltage;
                double current;
            } mc[2];
            // proximity sensors
            double prox_sig_limit;
            int prox_poll_intvl_us;
            struct {
                int enabled;
                int alert;
                double sig;
            } prox[2];
            // imu
            double mag_heading;
            double rotation;
            int accel_rot_enabled;
            int accel_alert_count;
            double accel_alert_last_value;
            double accel_alert_limit;
            // environment
            double temperature_degc;
            double pressure_pascal;
            double temperature_degf;
            double pressure_inhg;
            // buttons
            struct {
                int pressed;
            } button[2];
            // oled
            char oled_strs[MAX_OLED_STR][MAX_OLED_STR_SIZE];
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

