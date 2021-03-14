#include <string.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#include "i2c.h"
#include "../../util/misc.h"

#define I2C_DEVICE "/dev/i2c-1"

static int fd;

int i2c_init(void)
{
    // if I2C_DEVICE has already been opened successfully then return success
    if (fd > 0) {
        return 0;
    }

    // if previously failed to open I2C_DEVICE then return error
    if (fd < 0) {
        return -1;
    }

    // open I2C_DEVICE
    fd = open(I2C_DEVICE, O_RDWR);
    if (fd < 0) {
        ERROR("open %s, %s\n", I2C_DEVICE, strerror(errno));
        return -1;
    }

    // success
    return 0;
}

int i2c_read(int dev_addr, uint8_t reg_addr, uint8_t *reg_data, int len)
{
    struct i2c_msg messages[] = {
        { dev_addr, 0,        1,   &reg_addr },
        { dev_addr, I2C_M_RD, len, reg_data  } };
    struct i2c_rdwr_ioctl_data ioctl_data = { messages, 2 };
    int rc;

    if (fd <= 0) {
        ERROR("not initialized\n");
        return -1;
    }

    rc = ioctl(fd, I2C_RDWR, &ioctl_data);
    if (rc < 0) {
        ERROR("ioctl I2C_RDWR, %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

int i2c_write(int dev_addr, uint8_t reg_addr, uint8_t * reg_data, int len)
{
    uint8_t tmp[100];
    struct i2c_msg messages[] = {
        { dev_addr, 0, len+1, tmp  } };
    struct i2c_rdwr_ioctl_data ioctl_data = { messages, 1 };
    int rc;

    if (fd <= 0) {
        ERROR("not initialized\n");
        return -1;
    }

    if (len+1 > sizeof(tmp)) {
        ERROR("len %d too large\n", len);
        return -1;
    }

    tmp[0] = reg_addr;
    memcpy(tmp+1, reg_data, len);

    rc = ioctl(fd, I2C_RDWR, &ioctl_data);
    if (rc < 0) {
        ERROR("ioctl I2C_RDWR, %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

void i2c_delay_ns(unsigned int ns)
{
    struct timespec req;
    struct timespec rem;
    int rc;

    req.tv_sec  = (ns / 1000000000);
    req.tv_nsec = (ns % 1000000000);

    while ((rc = nanosleep(&req, &rem)) && errno == EINTR) {
        req = rem;
    }
    if (rc) {
        ERROR("nanosleep, %s\n", strerror(errno));
    }
}

