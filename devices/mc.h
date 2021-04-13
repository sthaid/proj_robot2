#ifndef __MC_H__
#define __MC_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>

//
// defines: variable identifiers
//

// status flag variables
#define VAR_ERROR_STATUS                  0
#define VAR_ERRORS_OCCURRED               1
#define VAR_SERIAL_ERRORS_OCCURRED        2
#define VAR_LIMIT_STATUS                  3
#define VAR_RESET_FLAGS                   127
// diagnostic variables
#define VAR_TARGET_SPEED                  20   // SIGNED
#define VAR_CURRENT_SPEED                 21   // SIGNED
#define VAR_BRAKE_AMOUNT                  22
#define VAR_INPUT_VOLTAGE                 23   // mV
#define VAR_TEMPERATURE_A                 24   // 0.1 C
#define VAR_TEMPERATURE_B                 25   // 0.1 C
#define VAR_BAUD_RATE_REGISTER            27   // BAUD = 72000000 / BRR
#define VAR_UP_TIME_LOW                   28   // ms
#define VAR_UP_TIME_HIGH                  29   // 65536 ms
// motor limit variables
#define VAR_MAX_SPEED_FORWARD             30
#define VAR_MAX_ACCEL_FORWARD             31
#define VAR_MAX_DECEL_FORWARD             32
#define VAR_BRAKE_DUR_FORWARD             33
#define VAR_STARTING_SPEED_FORWARD        34
#define VAR_MAX_SPEED_REVERSE             36
#define VAR_MAX_ACCEL_REVERSE             37
#define VAR_MAX_DECEL_REVERSE             38
#define VAR_BRAKE_DUR_REVERSE             39
#define VAR_STARTING_SPEED_REVERSE        40
// current limittng and measurement variables
#define VAR_CURRENT_LIMIT                 42   // internal units
#define VAR_RAW_CURRENT                   43   // internal units
#define VAR_CURRENT                       44   // mA
#define VAR_CURRENT_LIMITING_CONSEC_CNT   45   // number of consecutive 10ms intvls of current limiting
#define VAR_CURRENT_LIMITTING_OCCUR_CNT   46   // number of 10 ms intvls that current limit was activated

//
// defines: motor limit identifiers
// - the update period is set in the GUI, the default is 1 ms
//

#define MTRLIM_MAX_SPEED_FWD_AND_REV      0    // 0-3200
#define MTRLIM_MAX_ACCEL_FWD_AND_REV      1    // delta speed per update period
#define MTRLIM_MAX_DECEL_FWD_AND_REV      2    // delta speed per update period
#define MTRLIM_BRAKE_DUR_FWD_AND_REV      3    // 4 ms
#define MTRLIM_MAX_SPEED_FORWARD          4    // 0-3200
#define MTRLIM_MAX_ACCEL_FORWARD          5    // delta speed per update period
#define MTRLIM_MAX_DECEL_FORWARD          6    // delta speed per update period
#define MTRLIM_BRAKE_DUR_FORWARD          7    // 4 ms
#define MTRLIM_MAX_SPEED_REVERSE          8    // 0-3200
#define MTRLIM_MAX_ACCEL_REVERSE          9    // delta speed per update period
#define MTRLIM_MAX_DECEL_REVERSE         10    // delta speed per update period
#define MTRLIM_BRAKE_DUR_REVERSE         11    // 4 ms

//
// prototypes
//

int mc_init(int max_info, ...);

int mc_enable(int id);
int mc_status(int id, int *error_status);

int mc_speed(int id, int speed);
int mc_brake(int id);
int mc_coast(int id);
int mc_stop(int id);

int mc_get_variable(int id, int variable_id, int *value);
int mc_set_motor_limit(int id, int limit_id, int value);
int mc_set_current_limit(int id, int milli_amps);

int mc_get_fw_ver(int id, int *product_id, int *fw_ver_maj_bcd, int *fw_ver_min_bcd);

#ifdef __cplusplus
}
#endif

#endif
