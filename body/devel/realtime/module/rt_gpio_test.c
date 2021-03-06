#include <linux/module.h>
#include <linux/io.h>
#include <linux/delay.h>

#include <gpio.h>
#include <timer.h>

//
// defines
//

#define GPIO_PIN 5

//
// typedefs
//

//
// variables
//

static volatile bool gpiotest_exit_request;
static bool          gpiotest_exitted;

//
// prototypes
//

// the new kernel API
extern int run_offline_cpu(int cpu, void (*proc)(void));

int gpiotest_init(void);
void gpiotest_exit(void);

static void gpiotest_proc(void);

//
// module info
//
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Steven Haid");
MODULE_VERSION("1.0.0");

module_init(gpiotest_init);
module_exit(gpiotest_exit);

// -----------------  MODULE INIT & EXIT  -------------------------------------

int gpiotest_init(void)
{
    int rc;

    // init
    timer_init();
    gpio_init();
    set_gpio_func(GPIO_PIN, FUNC_OUT);

    // run gpiotest_proc on cpu 3
    rc = run_offline_cpu(3, gpiotest_proc);
    if (rc != 0) {
        printk("ERROR run_offline_cpu rc=%d\n", rc);
        timer_exit();
        gpio_exit();
        return rc;
    }

    // return success
    printk("gpiotest_init complete\n");
    return 0;
}

void gpiotest_exit(void)
{
    // cause gpiotest_proc to return
    gpiotest_exit_request = true;
    while (gpiotest_exitted == false) {
        msleep(100);
    }
    msleep(100);

    // exit  
    timer_exit();
    gpio_exit();

    // done
    printk("gpiotest_exit complete\n");
}

// -----------------  GPIOTEST PROC ------------------------------------------

// test:   square wave with 2 us period
// result: good square wave, with some jitter
static void gpiotest_proc(void)
{
    while (true) {
        // set gpio GPIO_PIN and delay until the next hardware timer microsec
        gpio_write(GPIO_PIN, 1);
        timer_delay(0);

        // clear gpio GPIO_PIN and delay until the next hardware timer microsec
        gpio_write(GPIO_PIN, 0);
        timer_delay(0);

        // check if time to exit
        if (gpiotest_exit_request) {
            break;
        }
    }

    // ack that this routine is exitting
    gpiotest_exitted = true;
}

