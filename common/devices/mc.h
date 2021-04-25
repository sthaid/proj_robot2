#ifndef __MC_H__
#define __MC_H__

#ifdef __cplusplus
extern "C" {
#endif

// notes regarding accel:
// - Value    Time_0_to_3200
//      1           3200 ms
//      10           320 ms
//   Time_0_to_3200 = 3200 / value
// - The default Speed Update Period is 1 ms,
//   and is found GUI Motor Settings tab

#define MC_STATE_ENABLED   1
#define MC_STATE_DISABLED  2

#define MC_STATE_STR(_state) \
    ((_state) == MC_STATE_ENABLED  ? "MC_STATE_ENABLED" : \
     (_state) == MC_STATE_DISABLED ? "MC_STATE_DISABLED" : \
                                     "MC_????")

typedef struct {
    int state;
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

// Notes:
// - call to mc_set_speed_all must supply speeds for all instances
// - Enabling debug_mode will cause the debug_mode_mtr_vars to be 
//   read at 100 ms interval, even when in MC_STATE_QUIESCED or 
//   MC_STATE_ERROR. This increases power usage.

int mc_init(int max_info, ...);  // these return -1 on error, else 0
int mc_enable_all(void);
void mc_disable_all(void);
int mc_set_speed(int id, int speed);
int mc_set_speed_all(int speed0, ...);

void mc_set_accel(int accel, int decel);
mc_status_t *mc_get_status(void);
void mc_debug_mode(bool enable);

#ifdef __cplusplus
}
#endif

#endif
