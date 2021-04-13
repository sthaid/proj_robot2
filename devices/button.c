#define _GNU_SOURCE

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <sched.h>
#include <string.h>
#include <errno.h>

#include <button.h>
#include <gpio.h>
#include <misc.h>

static struct info_s {
    int gpio;
    bool last_state;
    bool pressed;
} info_tbl[10];
int max_info;

static void *button_thread(void *cx);

// -----------------  API  ---------------------------------------------

int button_init(int max_info_arg, ...)   // int gpio_pin, ...
{
    static pthread_t tid;
    va_list ap;

    // check that button_init has not already been called
    if (tid) {
        ERROR("already initialized\n");
        return -1;
    }

    // init gpio
    if (gpio_init() < 0) {
        ERROR("gpio_init failed\n");
        return -1;
    }

    // save hardware info
    va_start(ap, max_info_arg);
    for (int i = 0; i < max_info_arg; i++) {
        int gpio_pin = va_arg(ap, int);
        info_tbl[i].gpio = gpio_pin;
    }
    max_info = max_info_arg;
    va_end(ap);

    // create the thread to process the button gpio sig values, and to
    // keep track of accumulated shaft position
    pthread_create(&tid, NULL, button_thread, NULL);

    // success
    return 0;
}

int button_pressed(int id)
{
    struct info_s *x = &info_tbl[id];
    bool ret;

    ret = x->pressed;
    x->pressed = false;
    return ret;
}

// -----------------  THREAD--------------------------------------------

static void *button_thread(void *cx)
{
    int rc, id;
    unsigned int gpio_all;
    struct sched_param param;
    cpu_set_t cpu_set;

    // set affinity to cpu 3
    CPU_ZERO(&cpu_set);
    CPU_SET(3, &cpu_set);
    rc = sched_setaffinity(0,sizeof(cpu_set_t),&cpu_set);
    if (rc < 0) {
        FATAL("sched_setaffinity, %s\n", strerror(errno));
    }

    // set realtime priority
    memset(&param, 0, sizeof(param));
    param.sched_priority = 85;
    rc = sched_setscheduler(0, SCHED_FIFO, &param);
    if (rc < 0) {
        FATAL("sched_setscheduler, %s\n", strerror(errno));
    }

    // loop forever
    while (true) {
        // read all gpio pins
        gpio_all = gpio_read_all();

        // loop over defined buttons, and determine if the button has just been pressed
        for (id = 0; id < max_info; id++) {
            struct info_s *x = &info_tbl[id];
            bool curr_state;

            curr_state = IS_BIT_CLR(gpio_all, x->gpio);
            if (curr_state && !x->last_state) {
                x->pressed = true;
            }
            x->last_state = curr_state;
        }

        // sleep for 10 ms
        usleep(10000);
    }

    return NULL;
}
