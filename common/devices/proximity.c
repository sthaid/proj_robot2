#define _GNU_SOURCE

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <sched.h>
#include <string.h>
#include <errno.h>

#include <proximity.h>
#include <gpio.h>
#include <timer.h>
#include <misc.h>

//
// defines
//

//
// variables
//

static struct info_s {
    int gpio_sig;
    int gpio_enable;
    double avg_sig;
} info_tbl[10];
static int max_info;
static int poll_rate;

//
// prototypes
//

static void *proximity_thread(void *cx);

// -----------------  API  ---------------------------------------------

int proximity_init(int max_info_arg, ...)  // int gpio_sig, int gpio_enable
{
    static pthread_t tid;
    int id;
    va_list ap;

    // sanity check that proximity_init has not already been called
    if (tid) {
        ERROR("already initialized\n");
        return -1;
    }

    // init timer and gpio functions
    if (gpio_init() < 0) {
        ERROR("gpio_init failed\n");
        return -1;
    }
    if (timer_init() < 0) {
        ERROR("timer_init failed\n");
        return -1;
    }

    // save hardware info
    va_start(ap, max_info_arg);
    for (int i = 0; i < max_info_arg; i++) {
        info_tbl[i].gpio_sig = va_arg(ap, int);
        info_tbl[i].gpio_enable = va_arg(ap, int);
    }
    max_info = max_info_arg;
    va_end(ap);

    // set GPIO_ENABLE to output, and disable the IR LED
    for (id = 0; id < max_info; id++) {
        struct info_s * info = &info_tbl[id];
        set_gpio_func(info->gpio_enable, FUNC_OUT);
        gpio_write(info->gpio_enable, 0);
    }

    // create the thread to process the proximity gpio sig values, and to
    // keep track of accumulated shaft position
    pthread_create(&tid, NULL, proximity_thread, NULL);

    // success
    return 0;
}

bool proximity_check(int id, double *avg_sig_arg, int *poll_rate_arg)
{
    struct info_s * info = &info_tbl[id];

    if (avg_sig_arg) *avg_sig_arg = info->avg_sig;
    if (poll_rate_arg) *poll_rate_arg = poll_rate;

    return info->avg_sig > 0.1;
}

void proximity_enable(int id)
{
    struct info_s * info = &info_tbl[id];

    gpio_write(info->gpio_enable, 1);
}

void proximity_disable(int id)
{
    struct info_s * info = &info_tbl[id];

    gpio_write(info->gpio_enable, 0);
}

// -----------------  THREAD--------------------------------------------

static void *proximity_thread(void *cx)
{
    int rc, id;
    unsigned int gpio_all;
    struct timespec ts;
    struct sched_param param;
    cpu_set_t cpu_set;
    int poll_count=0;
    uint64_t t_now, t_last=timer_get();

    // set affinity to cpu 3
    CPU_ZERO(&cpu_set);
    CPU_SET(3, &cpu_set);
    rc = sched_setaffinity(0,sizeof(cpu_set_t),&cpu_set);
    if (rc < 0) {
        FATAL("sched_setaffinity, %s\n", strerror(errno));
    }

    // set realtime priority
    memset(&param, 0, sizeof(param));
    param.sched_priority = 90;
    rc = sched_setscheduler(0, SCHED_FIFO, &param);
    if (rc < 0) {
        FATAL("sched_setscheduler, %s\n", strerror(errno));
    }

    // loop forever
    while (true) {
        // read all gpio pins
        gpio_all = gpio_read_all();

        // loop over proximity sensors
        for (id = 0; id < max_info; id++) {
            struct info_s * info = &info_tbl[id];

            int sig = ((gpio_all >> info->gpio_sig) & 1) ? 0 : 1;
            if (sig == 0) {
                info->avg_sig = 0.99 * info->avg_sig;
            } else {
                info->avg_sig = 0.99 * info->avg_sig + 0.01;
            }
        }

        // used to confirm poll rate
        poll_count++;
        t_now = timer_get();
        if (t_now > t_last + 1000000) {
            poll_rate = 1000000LL * poll_count / (t_now - t_last);
            t_last = t_now;
            poll_count = 0;
        }

        // sleep for 90 us
        ts.tv_sec = 0;
        ts.tv_nsec = 90000;  //  90 us,  0.090 ms
        nanosleep(&ts, NULL);
    }

    return NULL;
}
