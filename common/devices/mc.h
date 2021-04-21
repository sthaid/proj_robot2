#ifndef __MC_H__
#define __MC_H__

#ifdef __cplusplus
extern "C" {
#endif

// notes regarding accel:
//    Value    Time_0_to_3200
//      1           3200 ms
//      10           320 ms
//
//    Time_0_to_3200 = 3200 / value


#define MC_STATE_QUIESCED   1
#define MC_STATE_RUNNING    2
#define MC_STATE_ERROR      3

#define MC_STATE_STR(_state) \
    ((_state) == MC_STATE_RUNNING  ? "MC_STATE_RUNNING"  : \
     (_state) == MC_STATE_ERROR    ? "MC_STATE_ERROR"    : \
     (_state) == MC_STATE_QUIESCED ? "MC_STATE_QUIESCED" : \
                                     "MC_????")

typedef struct {
    int state;
    char reason_str[80];   // the reason for entering 'state'
    double voltage;
    double motors_current;
    int target_speed[10];
    bool debug_mode_enabled;
    struct debug_mode_mtr_vars_s {
        int error_status;
        int target_speed;
        int current_speed;
        int max_accel;
        int max_decel;
        int input_voltage;
        int current;
    } debug_mode_mtr_vars[10];
} mc_status_t;

int mc_init(int max_info, ...);  // char *devname, ...
int mc_enable_all(void);
int mc_set_speed(int id, int speed);
int mc_set_speed_all(int speed0, ...);
void mc_emergency_stop_all(char *reason_str);
void mc_set_accel(int normal_accel, int emer_stop_accel);
mc_status_t *mc_get_status(void);

// Enabling debug_mode will cause the debug_mode_mtr_vars to be 
// read at 100 ms interval, even when in MC_STATE_QUIESCED or 
// MC_STATE_ERROR. This increases power usage.
void mc_debug_mode(bool enable);

#ifdef __cplusplus
}
#endif

#endif
