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

// msg drive_proc, proc_id values
#define DRIVE_SCAL     1   // scal [feet] - drive straight calibration
#define DRIVE_MCAL     2   // mcal [secs] - magnetometer calibration
#define DRIVE_FWD     11   // fwd [feet] [mph] - go straight forward
#define DRIVE_REV     12   // rev [feet] [mph] - go straight reverse
#define DRIVE_ROT     13   // rot [degress] [fudge] - rotate 
#define DRIVE_HDG     14   // hdg [heading] [fudge] - rotate to magnetic heading
#define DRIVE_RAD     15   // rad [degrees] [radius_feet] [fudge] - drive forward with turn radius
#define DRIVE_TST1   101   // tst1 - repeat drive fwd/rev for range of mph, range 0.3 to 0.8  
#define DRIVE_TST2   102   // tst2 [fudge] - drive fwd, turn around and return to start point, using gyro
#define DRIVE_TST3   103   // tst3 [fudge] - drive fwd, turn around and return to start point, using magnetometer
#define DRIVE_TST4   104   // tst4 [cycles] [fudge] - repeating figure eight
#define DRIVE_TST5   105   // tst5 [cycles] [fudge] - repeating oval
#define DRIVE_TST6   106   // tst6 [cycles] [fudge] - repeating square, corner turn radius = 1
#define DRIVE_TST7   107   // tst7 [cycles] [fudge] - repeating square, corner turn radius = 0

typedef struct {
    int id;
    union {
        struct msg_drive_proc_s {
            int proc_id;
            int unique_id;
	    double arg[8];
        } drive_proc;
        struct drive_proc_complete_s {
            int unique_id;
            bool succ;
            char failure_reason[MAX_DRIVE_PROC_COMPLETE_REASON_STR_SIZE];
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

