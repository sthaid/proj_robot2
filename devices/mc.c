#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
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

//
// typedefs
//

//
// variables
//

static struct info_s {
    char *devname;
    int fd;
} info_tbl[] = {
        { "/dev/ttyACM0", -1 },
                    };

//
// prototypes
//

static void *monitor_thread(void *cx);

static int issue_cmd(int id, unsigned char *cmd, int cmdlen, unsigned char *resp, int resplen);
static int open_serial_port(const char * device, uint32_t baud_rate);
static int write_port(int id, const uint8_t * buffer, size_t size);
static ssize_t read_port(int id, uint8_t * buffer, size_t size);

static void crc_init(void);
static unsigned char crc(unsigned char message[], unsigned char length);

// -----------------  INIT API  --------------------------------------------

int mc_init(void)
{
    static pthread_t tid;
    int id, error_status;

    // sanity check MAX_MC, which is defined in mc.h
    if (MAX_MC != (sizeof(info_tbl) / sizeof(struct info_s))) {
        FATAL("define MAX_MC is incorrect\n");
    }

    // sanity check that mc_init has not already been called
    if (tid) {
        ERROR("already initialized\n");
        return -1;
    }

    // init crc table
    crc_init();

    // open the motor ctlrs
    for (id = 0; id < MAX_MC; id++) {
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

    // create monitor_thread to periodically check status of this mc
    pthread_create(&tid, NULL, monitor_thread, NULL);

    // success
    return 0;
}

// -----------------  RUN TIME API  ----------------------------------------

// enable by exit safe start
int mc_enable(int id)
{
    unsigned char cmd[1] = { 0x83 };
    int error_status;

    // exit safe start
    if (issue_cmd(id, cmd, sizeof(cmd), NULL, 0) < 0) {
        return -1;
    }

    // return success if error_status is 0, else failure
    if (mc_status(id, &error_status) < 0) {
        return -1;
    }
    return error_status == 0 ? 0 : -1;
}

// get VAR_ERROR_STATUS
int mc_status(int id, int *error_status)
{
    if (mc_get_variable(id, VAR_ERROR_STATUS, error_status) < 0) {
        return -1;
    }
    return 0;
}

// set speed, forward or reverse based on speed arg
int mc_speed(int id, int speed)
{
    if (speed >= 0) {
        return issue_cmd(id, (unsigned char []){0x85, speed & 0x1f, speed >> 5 & 0x7f}, 3, NULL, 0);
    } else {
        speed = -speed;
        return issue_cmd(id, (unsigned char []){0x86, speed & 0x1f, speed >> 5 & 0x7f}, 3, NULL, 0);
    }
}

// immedeate brake, ignore decel limit
int mc_brake(int id)
{
    return issue_cmd(id, (unsigned char []){0x92, 0x20}, 2, NULL, 0);
}

// immedeate coast, ignore decel limit
int mc_coast(int id)
{
    return issue_cmd(id, (unsigned char []){0x92, 0x00}, 2, NULL, 0);
}

// stop motor, observe decel limit, enter safe start violation
int mc_stop(int id)
{
    return issue_cmd(id, (unsigned char []){0xe0}, 1, NULL, 0);
}

// get variable's value
int mc_get_variable(int id, int variable_id, int *value)
{
    unsigned char cmd[2] = { 0xa1, variable_id };
    unsigned char resp[2];
    bool value_is_signed;

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
int mc_set_motor_limit(int id, int limit_id, int value)
{
    unsigned char resp[1];

    if (issue_cmd(id, (unsigned char []){0xa2, limit_id, value & 0x7f, value >> 7}, 4, resp, sizeof(resp)) < 0) {
        return -1;
    }

    return resp[0] == 0 ? 0 : -1;
}

#if 0  // instead, configure the current limit using the GUI
// return 0 if cmd was successfully issued, else return -1
// notes: this routine uses default calibration values, so won't be accurate
int mc_set_current_limit(int id, int milli_amps)
{
    int limit;
    int current_scale_cal  = 8057;  // default
    int current_offset_cal = 993;   // default
    unsigned char resp[1];

    limit = (milli_amps * 3200 * 2 / current_scale_cal + current_offset_cal) * 3200 / 65536;

    return issue_cmd(id, 
                     (unsigned char []){0x91, limit & 0x7f, limit >> 7}, 3, 
                     resp, sizeof(resp));
}
#endif

// return 0 if cmd was successfully issued, else return -1;
// when 0 is returned the product_id and fw_ver values have been set
int mc_get_fw_ver(int id, int *product_id, int *fw_ver_maj_bcd, int *fw_ver_min_bcd)
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

// -----------------  MONITOR THREAD  --------------------------------------

// XXX need a way to propogate failure info
static void *monitor_thread(void *cx)
{
    int  id;
    bool stop_all_motors;
    int  error_status, error_status_last[MAX_MC];
    int curr_limit_cnt;

    for (id = 0; id < MAX_MC; id++) {
        error_status_last[id] = 1;  // Safe Start Violation
    }

    while (true) {
        stop_all_motors = false;
        for (id = 0; id < MAX_MC; id++) {
            // get error status 
            error_status = -1;
            mc_status(id, &error_status);

            // if not okay now, and was okay last time then stop all motors
            if (error_status != 0 && error_status_last[id] == 0) {
                ERROR("stop_all_motors, id=%d error_status=0x%x\n", id, error_status);
                stop_all_motors = true;
                break;
            }
            error_status_last[id] = error_status;

            // check for active current limitting; if so then stop all motors
            // Variable description:
            //   The number of 10 ms time periods in which the hardware
            //   current limit has activated since the last time this variable
            //   was cleared. Reading this variable clears it, resetting it to 0.
            curr_limit_cnt = 999;
            mc_get_variable(id, VAR_CURRENT_LIMITTING_OCCUR_CNT, &curr_limit_cnt);
            if (curr_limit_cnt > 10) {
                ERROR("stop_all_motors, id=%d curr_limit_cnt=%d\n", id, curr_limit_cnt);
                stop_all_motors = true;
                break;
            }
        }

        // if something has gone wrong then stop all motors
        if (stop_all_motors) {
            for (id = 0; id < MAX_MC; id++) {
                mc_stop(id);
            }
        }

        // sleep for 250 ms
        usleep(250000);
        continue;
    }

    return NULL;
}

// -----------------  SEND COMMAND  ----------------------------------------

static int issue_cmd(int id, unsigned char *cmd, int cmdlen, unsigned char *resp, int resplen)
{
    unsigned char lcl_cmd[64], lcl_resp[64];
    int rc;
    char err_str[100];

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

    // return success
    return 0;

err:
    // print and return error
    ERROR("id=%d cmd=%s - %s\n", id, CMD_STR(cmd[0]), err_str);
    return -1;
}

// -----------------  SERIAL PORT  -----------------------------------------

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

// -----------------  CRC  -------------------------------------------------

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
