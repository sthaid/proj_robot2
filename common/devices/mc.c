#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <termios.h>

#include <mc.h>
#include <misc.h>

//
// defines
//

// default accel/decel settings
#define DEFAULT_ACCEL   5

// convert mtr ctlr cmd value to string
#define CMD_STR(x) \
   ((x) == 0x83 ? "EXIT_SAFE_START"      : \
    (x) == 0x85 ? "MOTOR_FORWARD"        : \
    (x) == 0x86 ? "MOTOR_REVERSE"        : \
    (x) == 0x92 ? "MOTOR_BRAKE_OR_COAST" : \
    (x) == 0xa1 ? "GET_VARIABLE"         : \
    (x) == 0xa2 ? "SET_MOTOR_LIMIT"      : \
    (x) == 0x91 ? "SET_CURRENT_LIMIT"    : \
    (x) == 0xc1 ? "GET_FW_VER"           : \
    (x) == 0xe0 ? "STOP_MOTOR"           : \
                  "????")

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

// defines: motor limit identifiers
// - the update period is set in the GUI, the default is 1 ms
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

// misc
#define SECS_TO_US(secs)  ((secs) * 1000000)

#define SET_STATE(_state) \
    do { \
        status.state = (_state); \
    } while (0)

//
// typedefs
//

//
// variables
//

static struct info_s {
    char devname[100];   // for example: "/dev/ttyACM0"
    int fd;
    int accel;
    int decel;
    int target_speed;
    struct {
        int error_status;
        int curr_limit_cnt;
        int input_voltage;
        int current;
    } vars;
} info_tbl[10];
static int max_info;

static mc_status_t     status;
static int             accel  = DEFAULT_ACCEL;
static int             decel  = DEFAULT_ACCEL;
static pthread_mutex_t mutex  = PTHREAD_MUTEX_INITIALIZER;

//
// prototypes
//

static int mc_enable(int id);
static int mc_speed(int id, int speed);
static int mc_stop(int id);
static int mc_get_variable(int id, int variable_id, int *value);
static int mc_set_motor_limit(int id, int limit_id, int value);
static int mc_get_fw_ver(int id, int *product_id, int *fw_ver_maj_bcd, int *fw_ver_min_bcd) __attribute__((unused));

static void *monitor_thread(void *cx);
static bool any_mc_error_indication(char *reason_str, int reason_str_size);

static int issue_cmd(int id, unsigned char *cmd, int cmdlen, unsigned char *resp, int resplen);
static int open_serial_port(const char * device, uint32_t baud_rate);
static int write_port(int id, const uint8_t * buffer, size_t size);
static ssize_t read_port(int id, uint8_t * buffer, size_t size);

static void crc_init(void);
static unsigned char crc(unsigned char message[], unsigned char length);

// -----------------  INIT API  --------------------------------------------

int mc_init(int max_info_arg, ...)  // char *devname, ...
{
    static pthread_t tid;
    int id, error_status;
    va_list ap;

    // check that mc_init has not already been called
    if (tid) {
        ERROR("already initialized\n");
        return -1;
    }

    // save hardware info
    va_start(ap, max_info_arg);
    for (int i = 0; i < max_info_arg; i++) {
        strcpy(info_tbl[i].devname, va_arg(ap, char*));
        info_tbl[i].fd = -1;
    }
    max_info = max_info_arg;
    va_end(ap);

    // init crc table
    crc_init();

    // open the motor ctlrs
    for (id = 0; id < max_info; id++) {
        struct info_s * info = &info_tbl[id];

        // open usb serial devname
        // note: baud rate doesn't matter because we are connecting to the SMC over USB
        info->fd = open_serial_port(info->devname, 9600);
        if (info->fd < 0) {
            return -1;
        }

        // get error status, to confirm we can communicate to the ctlr
        if (mc_get_variable(id, VAR_ERROR_STATUS, &error_status) < 0) {
            close(info->fd);
            info->fd = -1;
            return -1;
        }
    }

    // initialize state to DISABLED
    SET_STATE(MC_STATE_DISABLED);

    // create monitor_thread to periodically check status of this mc
    pthread_create(&tid, NULL, monitor_thread, NULL);

    // success
    return 0;
}

// -----------------  API - ENABLE & DISABLE ROUTINES  ---------------------

int mc_enable_all(void)
{
    int id;

    // acquire mutex
    pthread_mutex_lock(&mutex);

    // init some info_tbl[] fields
    for (id = 0; id < max_info; id++) {
        info_tbl[id].target_speed = 0;
        status.target_speed[id] = 0;
    }

    // enable all mtr-ctlrs
    for (id = 0; id < max_info; id++) {
        if (mc_enable(id) < 0) {
            for (int i = 0; i < id; i++) {
                mc_stop(i);
            }
            SET_STATE(MC_STATE_DISABLED);
            pthread_mutex_unlock(&mutex);
            return -1;
        }
    }

    // set state to ENABLED
    SET_STATE(MC_STATE_ENABLED);

    // release mutex
    pthread_mutex_unlock(&mutex);

    // success
    return 0;
}

void mc_disable_all(void)
{
    // acquire mutex
    pthread_mutex_lock(&mutex);

    // stop all motors
    for (int id = 0; id < max_info; id++) {
        struct info_s *x = &info_tbl[id];

        x->target_speed = 0;
        status.target_speed[id] = 0;

        if (x->decel != decel) {
            mc_set_motor_limit(id, MTRLIM_MAX_DECEL_FWD_AND_REV, decel);
            x->decel = decel;
        }

        mc_stop(id);
    }

    // set state to DISABLED
    SET_STATE(MC_STATE_DISABLED);
    status.motors_current = 0;

    // release mutex
    pthread_mutex_unlock(&mutex);
}

// -----------------  API - SET SPEED ROUTINES  ----------------------------

int mc_set_speed(int id, int speed)
{
    struct info_s *x = &info_tbl[id];

    // acquire mutex
    pthread_mutex_lock(&mutex);

    // must be enabled
    if (status.state != MC_STATE_ENABLED) {
        ERROR("state must be MC_STATE_ENABLED\n");
        pthread_mutex_unlock(&mutex);
        return -1;
    }

    // keep track of the requested speed, so that the monitor_thread
    // can determine all motors are stopped and set state to quiesced
    x->target_speed = speed;
    status.target_speed[id] = speed;

    // ensure that the motor limit accel/decel variables are set
    if (x->accel != accel) {
        mc_set_motor_limit(id, MTRLIM_MAX_ACCEL_FWD_AND_REV, accel);
        x->accel = accel;
    } 
    if (x->decel != decel) {
        mc_set_motor_limit(id, MTRLIM_MAX_DECEL_FWD_AND_REV, decel);
        x->decel = decel;
    } 

    // set the motor's speed
    mc_speed(id, speed);

    // release mutex
    pthread_mutex_unlock(&mutex);

    return 0;
}

int mc_set_speed_all(int speed0, ...)
{
    va_list ap;
    int id;

    pthread_mutex_lock(&mutex);

    if (status.state != MC_STATE_ENABLED) {
        ERROR("state must be MC_STATE_ENABLED\n");
        pthread_mutex_unlock(&mutex);
        return -1;
    }

    va_start(ap, speed0);
    for (id = 0; id < max_info; id++) {
        struct info_s *x = &info_tbl[id];
        int speed = (id == 0 ? speed0 : va_arg(ap, int));

        x->target_speed = speed;
        status.target_speed[id] = speed;

        if (x->accel != accel) {
            mc_set_motor_limit(id, MTRLIM_MAX_ACCEL_FWD_AND_REV, accel);
            x->accel = accel;
        } 
        if (x->decel != decel) {
            mc_set_motor_limit(id, MTRLIM_MAX_DECEL_FWD_AND_REV, decel);
            x->decel = decel;
        } 

        mc_speed(id, speed);
    }
    va_end(ap);

    pthread_mutex_unlock(&mutex);

    return 0;
}

// -----------------  API - MISC ROUTINES  ---------------------------------

void mc_set_accel(int accel_arg, int decel_arg)
{
    accel = accel_arg;
    decel = decel_arg;
}

mc_status_t *mc_get_status(void)
{
    return &status;
}

void mc_debug_mode(bool enable)
{
    status.debug_mode_enabled = enable;
}

// -----------------  MONITOR THREAD  --------------------------------------

static void *monitor_thread(void *cx)
{
    static uint64_t time_last_status_vin_update;

    while (true) {
        if (status.state == MC_STATE_ENABLED) {
            double motors_current;
            char error_str[80];
            int id;

            // read registers for all mtr ctlrs
            // - VAR_ERROR_STATUS
            // - VAR_CURRENT_LIMITTING_OCCUR_CNT
            // - VAR_INPUT_VOLTAGE
            // - VAR_CURRENT
            for (id = 0; id < max_info; id++) {
                struct info_s *x = &info_tbl[id];
                int error_status=0xffff, curr_limit_cnt=0, input_voltage=0, current=0;
                mc_get_variable(id, VAR_ERROR_STATUS, &error_status);
                mc_get_variable(id, VAR_CURRENT_LIMITTING_OCCUR_CNT, &curr_limit_cnt);
                mc_get_variable(id, VAR_INPUT_VOLTAGE, &input_voltage);
                mc_get_variable(id, VAR_CURRENT, &current);
                x->vars.error_status   = error_status;
                x->vars.curr_limit_cnt = curr_limit_cnt;
                x->vars.input_voltage  = input_voltage;
                x->vars.current        = current;
            }

            // determine voltage and total motors current values
            status.voltage = info_tbl[0].vars.input_voltage / 1000.;
            for (motors_current = 0, id = 0; id < max_info; id++) {
                motors_current += info_tbl[id].vars.current / 1000.;
            }
            status.motors_current = motors_current;

            // if registers indicate an error condition then
            //   call mc_disable_all, this changes state to DISABLED and 
            //    sets status.motor_current to 0
            // endif
            if (any_mc_error_indication(error_str,sizeof(error_str))) {
                ERROR("motor error: %s\n", error_str);
                mc_disable_all();
            }
        } else if (status.state == MC_STATE_DISABLED) {
            // update status.voltage every 10 secs
            uint64_t time_now = microsec_timer();
            if ((time_now - time_last_status_vin_update > SECS_TO_US(10)) ||
                (status.voltage == 0)) 
            {
                int mv = 0;
                mc_get_variable(0, VAR_INPUT_VOLTAGE, &mv);
                status.voltage = mv / 1000.;
                time_last_status_vin_update = time_now;
            }
        } else {
            FATAL("invalid state %d\n", status.state);
        }

        // if debug_mode_enabled then fill in the status.debug_mode_mtr_vars struct
        if (status.debug_mode_enabled) {
            for (int id = 0; id < max_info; id++) {
                struct debug_mode_mtr_vars_s *x = &status.debug_mode_mtr_vars[id];
                mc_get_variable(id, VAR_ERROR_STATUS, &x->error_status);
                mc_get_variable(id, VAR_TARGET_SPEED, &x->target_speed);
                mc_get_variable(id, VAR_CURRENT_SPEED, &x->current_speed);
                mc_get_variable(id, VAR_MAX_ACCEL_FORWARD, &x->max_accel);
                mc_get_variable(id, VAR_MAX_DECEL_FORWARD, &x->max_decel);
                mc_get_variable(id, VAR_INPUT_VOLTAGE, &x->input_voltage);
                mc_get_variable(id, VAR_CURRENT, &x->current);
            }
        }

        // sleep for 100 ms
        usleep(100000);
    }

    return NULL;
}

static bool any_mc_error_indication(char *error_str, int error_str_size)
{
    // Error Status (pg 96) -
    // - bit 0: Safe start violation
    // - bit 1: Required channel invalid
    // - bit 2: Serial Error
    // - bit 3: Command timeout
    // - bit 4: Limit/kill switch
    // - bit 5: Low VIN
    // - bit 6: High VIN
    // - bit 7: Over temperature
    // - bit 8: Motor driver error
    // - bit 9: ERR line high
    // 
    // Current Limiting Occurence Count (pg 103) - 
    //   The number of 10 ms time periods in which the hardware
    //   current limit has activated since the last time this 
    //   variable was cleared. 
    //   Reading this variable clears it, resetting it to 0.

    for (int id = 0; id < max_info; id++) {
        struct info_s *x = &info_tbl[id];
        if (x->vars.error_status) {
            snprintf(error_str, error_str_size, 
                     "id=%d error_status=0x%x", id, x->vars.error_status);
            return true;
        }
        if (x->vars.curr_limit_cnt > 3) {
            snprintf(error_str, error_str_size, 
                     "id=%d curr_limit_cnt=%d", id, x->vars.curr_limit_cnt);
            return true;
        }
        if (x->vars.curr_limit_cnt > 0) {
            INFO("XXX id = %d  curr_limit_cnt = %d\n", id, x->vars.curr_limit_cnt);
        }
    }

    error_str[0] = '\0';
    return false;
}

// -----------------  PRIVATE: INTFC TO MOTOR CTLR --------------------------

static int mc_enable(int id)
{
    unsigned char cmd[1] = { 0x83 };
    int error_status;

    // exit safe start
    if (issue_cmd(id, cmd, sizeof(cmd), NULL, 0) < 0) {
        ERROR("motor %d failed to exit safe start\n", id);
        return -1;
    }

    // return success if error_status is 0, else failure
    if (mc_get_variable(id, VAR_ERROR_STATUS, &error_status) < 0) {
        ERROR("motor %d has error_status 0x%x\n", id, error_status);
        return -1;
    }
    return error_status == 0 ? 0 : -1;
}

// set speed, forward or reverse based on speed arg
static int mc_speed(int id, int speed)
{
    if (speed >= 0) {
        return issue_cmd(id, (unsigned char []){0x85, speed & 0x1f, speed >> 5 & 0x7f}, 3, NULL, 0);
    } else {
        speed = -speed;
        return issue_cmd(id, (unsigned char []){0x86, speed & 0x1f, speed >> 5 & 0x7f}, 3, NULL, 0);
    }
}

// stop motor, observe decel limit, enter safe start violation
static int mc_stop(int id)
{
    return issue_cmd(id, (unsigned char []){0xe0}, 1, NULL, 0);
}

// get variable's value
static int mc_get_variable(int id, int variable_id, int *value)
{
    unsigned char cmd[2] = { 0xa1, variable_id };
    unsigned char resp[2];
    bool value_is_signed;

    //INFO("GET VARIABLE %d\n", variable_id);

    if (issue_cmd(id, cmd, sizeof(cmd), resp, sizeof(resp)) < 0) {
        return -1;
    }

    *value = resp[0] + 256 * resp[1];

    value_is_signed = (variable_id == VAR_TARGET_SPEED) ||
                      (variable_id == VAR_CURRENT_SPEED);
    if (value_is_signed && *value > 32767 ) {
        *value -= 65536;
    }

    return 0;
}

// set motor limit
static int mc_set_motor_limit(int id, int limit_id, int value)
{
    unsigned char resp[1];

    if (issue_cmd(id, (unsigned char []){0xa2, limit_id, value & 0x7f, value >> 7}, 4, resp, sizeof(resp)) < 0) {
        return -1;
    }

    return resp[0] == 0 ? 0 : -1;
}

// return 0 if cmd was successfully issued, else return -1;
// when 0 is returned the product_id and fw_ver values have been set
static int mc_get_fw_ver(int id, int *product_id, int *fw_ver_maj_bcd, int *fw_ver_min_bcd)
{
    unsigned char resp[4];

    if (issue_cmd(id, (unsigned char []){0xc2}, 1, resp, 4) < 0) {
        return -1;
    }

    *product_id = resp[0] + 256 * resp[1];
    *fw_ver_min_bcd = resp[2];
    *fw_ver_maj_bcd = resp[3];

    return 0;
}

// -----------------  PRIVATE: ISSUE CMD TO MTR CTLR  ----------------------

static int issue_cmd(int id, unsigned char *cmd, int cmdlen, unsigned char *resp, int resplen)
{
    unsigned char lcl_cmd[64], lcl_resp[64];
    int rc;
    char err_str[100];

    static pthread_mutex_t issue_cmd_mutex = PTHREAD_MUTEX_INITIALIZER;

    // acquire issue_cmd_mutex
    pthread_mutex_lock(&issue_cmd_mutex);

    // copy cmd to lcl_cmd, and append crc byte, and send
    memcpy(lcl_cmd, cmd, cmdlen);
    lcl_cmd[cmdlen] = crc(lcl_cmd, cmdlen);
    rc = write_port(id, lcl_cmd, cmdlen+1);
    if (rc < 0) {
        sprintf(err_str, "write_port");
        goto err; 
    }

    // if resplen then read the response, verify the crc, and 
    // copy the lcl_resp to caller's buffer
    if (resplen) {
        rc = read_port(id, lcl_resp, resplen+1);
        if (rc != resplen+1) {
            sprintf(err_str, "read_port rc=%d != exp=%d", rc, resplen+1);
            goto err; 
        }
        if (lcl_resp[resplen] != crc(lcl_resp, resplen)) {
            sprintf(err_str, "crc");
            goto err; 
        }
        memcpy(resp, lcl_resp, resplen);
    }

    // release issue_cmd_mutex
    pthread_mutex_unlock(&issue_cmd_mutex);

    // return success
    return 0;

err:
    // print and return error
    ERROR("id=%d cmd=%s - %s\n", id, CMD_STR(cmd[0]), err_str);
    pthread_mutex_unlock(&issue_cmd_mutex);
    return -1;
}

// -----------------  PRIVATE: SERIAL PORT  ---------------------------------

// https://www.pololu.com/docs/0J77/8.6
// 8.6. Example serial code for Linux and macOS in C

// Opens the specified serial port, sets it up for binary communication,
// configures its read timeouts, and sets its baud rate.
// Returns a non-negative file descriptor on success, or -1 on failure.
static int open_serial_port(const char * device, uint32_t baud_rate)
{
    int fd;

    fd = open(device, O_RDWR | O_NOCTTY);
    if (fd == -1) {
        ERROR("open %s, %s\n", device, strerror(errno));
        return -1;
    }
 
    // Flush away any bytes previously read or written.
    int result = tcflush(fd, TCIOFLUSH);
    if (result) {
        ERROR("tcflush %s, %s\n", device, strerror(errno));
    }
 
    // Get the current configuration of the serial port.
    struct termios options;
    result = tcgetattr(fd, &options);
    if (result) {
        ERROR("tcgetattr %s, %s\n", device, strerror(errno));
        close(fd);
        return -1;
    }
 
    // Turn off any options that might interfere with our ability to send and
    // receive raw binary bytes.
    options.c_iflag &= ~(INLCR | IGNCR | ICRNL | IXON | IXOFF);
    options.c_oflag &= ~(ONLCR | OCRNL);
    options.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
 
    // Set up timeouts: Calls to read() will return as soon as there is
    // at least one byte available or when 100 ms has passed.
    options.c_cc[VTIME] = 1;
    options.c_cc[VMIN] = 0;
 
    // This code only supports certain standard baud rates. Supporting
    // non-standard baud rates should be possible but takes more work.
    switch (baud_rate) {
    case 4800:   cfsetospeed(&options, B4800);   break;
    case 9600:   cfsetospeed(&options, B9600);   break;
    case 19200:  cfsetospeed(&options, B19200);  break;
    case 38400:  cfsetospeed(&options, B38400);  break;
    case 115200: cfsetospeed(&options, B115200); break;
    default:
        ERROR("baud rate %u is not supported, using 9600.\n", baud_rate);
        cfsetospeed(&options, B9600);
        break;
    }
    cfsetispeed(&options, cfgetospeed(&options));
 
    result = tcsetattr(fd, TCSANOW, &options);
    if (result) {
        ERROR("tcsetattr %s, %s\n", device, strerror(errno));
        close(fd);
        return -1;
    }
 
    return fd;
}
 
// Writes bytes to the serial port, returning 0 on success and -1 on failure.
static int write_port(int id, const uint8_t * buffer, size_t size)
{
    ssize_t result;
    struct info_s * info = &info_tbl[id];

    result = write(info->fd, buffer, size);
    if (result != (ssize_t)size) {
        ERROR("write %s, %s\n", info->devname, strerror(errno));
        return -1;
    }

    return 0;
}
 
// Reads bytes from the serial port.
// Returns after all the desired bytes have been read, or if there is a
// timeout or other error.
// Returns the number of bytes successfully read into the buffer, or -1 if
// there was an error reading.
static ssize_t read_port(int id, uint8_t * buffer, size_t size)
{
    size_t received = 0;
    struct info_s * info = &info_tbl[id];

    while (received < size) {
        ssize_t r = read(info->fd, buffer + received, size - received);
        if (r < 0) {
            ERROR("read %s, %s\n", info->devname, strerror(errno));
            return -1;
        }
        if (r == 0) {
            ERROR("read timedout %s\n", info->devname);
            break;
        }
        received += r;
    }
    return received;
}

// -----------------  PRIVATE: CRC  -----------------------------------------

// https://www.pololu.com/docs/0J77/8.13
// 8.13. Example CRC computation in C
 
static const unsigned char CRC7_POLY = 0x91;
static unsigned char crc_table[256];
 
static unsigned char get_crc_for_byte(unsigned char val)
{
    unsigned char j;
 
    for (j = 0; j < 8; j++) {
        if (val & 1)
            val ^= CRC7_POLY;
        val >>= 1;
    }
 
    return val;
}
 
static void crc_init()
{
    int i;
 
    // fill an array with CRC values of all 256 possible bytes
    for (i = 0; i < 256; i++) {
        crc_table[i] = get_crc_for_byte(i);
    }
}
 
static unsigned char crc(unsigned char message[], unsigned char length)
{
    unsigned char i, crc = 0;
 
    for (i = 0; i < length; i++)
        crc = crc_table[crc ^ message[i]];

    return crc;
}
