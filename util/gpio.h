#ifndef __GPIO_H__
#define __GPIO_H__

#ifdef __cplusplus
extern "C" {
#endif

// This code is for bcm2711.
//
// Reference https://datasheets.raspberrypi.org/bcm2711/bcm2711-peripherals.pdf

#ifndef __KERNEL__
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <misc.h>
#else
#include <linux/module.h>
#endif

#define PI4B_PERIPHERAL_BASE_ADDR 0xfe000000
#define GPIO_BASE_ADDR            (PI4B_PERIPHERAL_BASE_ADDR + 0x200000)
#define GPIO_BLOCK_SIZE           0x1000

#define PULL_NONE  0
#define PULL_UP    1
#define PULL_DOWN  2

#define FUNC_IN    0
#define FUNC_OUT   1

volatile unsigned int *gpio_regs;

// -----------------  GPIO: CONFIGURATION  ----------------

static inline int get_gpio_func(int pin)
{
    int regidx, bit;

    regidx = 0 + (pin / 10);
    bit = (pin % 10) * 3;

    return (gpio_regs[regidx] >> bit) & 7;
}

static inline void set_gpio_func(int pin, int func)
{
    int regidx, bit, curr_func;
    unsigned int tmp;
    
    curr_func = get_gpio_func(pin);
    if (curr_func != FUNC_IN && curr_func != FUNC_OUT) {
#ifndef __KERNEL__
        FATAL("can't change func for pin %d\n", pin);
#else
        pr_err("can't change func for pin %d\n", pin);
        return;
#endif
    }

    regidx = 0 + (pin / 10);
    bit = (pin % 10) * 3;

    tmp = gpio_regs[regidx];
    tmp &= ~(7 << bit);  // 3 bit field
    tmp |= (func << bit);

    gpio_regs[regidx] = tmp;
}

static inline void set_gpio_pull(int pin, int pull)
{
    int regidx, bit;
    unsigned int tmp;

    regidx = 57 + (pin / 16);
    bit = (pin % 16) * 2;

    tmp = gpio_regs[regidx];
    tmp &= ~(3 << bit);   // 2 bit field
    tmp |= (pull << bit);

    gpio_regs[regidx] = tmp;
}

// -----------------  GPIO: READ & WRITE  -----------------

// these gpio read/write routines support only gpio 0 to 31, which
// covers all of the gpios supported on the raspberry-pi header

static inline int gpio_read(int pin)
{
    return (gpio_regs[13] & (1 << pin)) != 0;
}

static inline unsigned int gpio_read_all(void)
{
    return gpio_regs[13];
}

static inline void gpio_write(int pin, int value)
{
    if (value) {
        gpio_regs[7] = (1 << pin);
    } else {
        gpio_regs[10] = (1 << pin);
    }
}

// -----------------  GPIO: INIT & EXIT  ------------------

static inline int gpio_init(void)
{
#ifndef __KERNEL__
    int fd, rc;
    int okay;
#endif

    // if already initialized the return success
    if (gpio_regs) {
        return 0;
    }

#ifndef __KERNEL__
    // verify bcm version
    rc = system("grep BCM2711 /proc/cpuinfo > /dev/null");
    okay = WIFEXITED(rc) && WEXITSTATUS(rc) == 0;
    if (!okay) {
        ERROR("this program requires BCM2711\n");
        return -1;
    }

    // map gpio regs
    if ((fd = open("/dev/mem", O_RDWR|O_SYNC) ) < 0) {
        ERROR("can't open /dev/mem \n");
        return -1;
    }
    gpio_regs = mmap(NULL,
                     GPIO_BLOCK_SIZE,
                     PROT_READ | PROT_WRITE,
                     MAP_SHARED,
                     fd,
                     GPIO_BASE_ADDR);
    if (gpio_regs == MAP_FAILED) {
        ERROR("mmap failed\n");
        return -1;
    }
    close(fd);
#else
    gpio_regs = ioremap(GPIO_BASE_ADDR, 0x1000);
#endif

    // GPIO in range 0..31 that are FUNC_IN or FUNC_OUT
    // are initialized to: FUNC_IN, PULL_DOWN, and output set to 0
    for (int pin = 0; pin < 32; pin++) {
        int func = get_gpio_func(pin);
        if (func == FUNC_IN || func == FUNC_OUT) {
            if (func == FUNC_OUT) {
                set_gpio_func(pin, FUNC_IN);
            }
            set_gpio_pull(pin, PULL_DOWN);
            gpio_write(pin, 0);
        }
    }

    // success
    return 0;
}

static inline void gpio_exit(void)
{
#ifndef __KERNEL__
    // program termination will cleanup
    // XXX should unmap
#else
    iounmap(gpio_regs);
#endif
}

#ifdef __cplusplus
}
#endif

#endif
