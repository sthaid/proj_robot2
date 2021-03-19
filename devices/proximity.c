#define _GNU_SOURCE

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <sched.h>
#include <string.h>
#include <errno.h>

#include <config_hw.h>
#include <proximity.h>
#include <gpio.h>
#include <timer.h>
#include <misc.h>

//
// defines
//

// when polling at 0.1 ms intervals then MAX_SIG of 64
// yields a 6.4 ms sensor averaging interval
#define MAX_SIG  64

//
// variables
//

static struct info_s {
    int gpio_sig;
    int gpio_enable;
    int sig[MAX_SIG];
    int tail;
    int sum;
} info_tbl[] = { 
        { PROXIMITY_FRONT_GPIO_SIG, PROXIMITY_FRONT_GPIO_ENABLE },
                           };

static int poll_rate;

//
// prototypes
//

static void *proximity_thread(void *cx);

// -----------------  API  ---------------------------------------------

int proximity_init(void)
{
    static pthread_t tid;
    int id;

    // sanity check MAX_PROXIMITY, which is defined in proximity.h
    if (MAX_PROXIMITY != (sizeof(info_tbl) / sizeof(struct info_s))) {
        FATAL("define MAX_PROXIMITY is incorrect\n");
    }

    // sanity check that proximity_init has not already been called
    if (tid) {
        ERROR("already initialized\n");
        return -1;
    }

    // set GPIO_ENABLE to output, and disable the IR LED
    for (id = 0; id < MAX_PROXIMITY; id++) {
        struct info_s * info = &info_tbl[id];
        set_gpio_func(info->gpio_enable, FUNC_OUT);
        gpio_write(info->gpio_enable, 0);   // XXX check if this is a disable
    }

    // create the thread to process the proximity gpio sig values, and to
    // keep track of accumulated shaft position
    pthread_create(&tid, NULL,*proximity_thread, NULL);

    // success
    return 0;
}

bool proximity_check(int id, int *sum_arg, int *poll_rate_arg)
{
    struct info_s * info = &info_tbl[id];
    int sum;

    if (id < 0 || id >= MAX_PROXIMITY) {
        FATAL("invalid id %d\n", id);
    }

    sum = info->sum;

    if (sum_arg) *sum_arg = sum;
    if (poll_rate_arg) *poll_rate_arg = poll_rate;
    
    return sum > (MAX_SIG/2);
}

// XXX api to enable/disable

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
        for (id = 0; id < MAX_PROXIMITY; id++) {
            struct info_s * info = &info_tbl[id];
            int sig, new_tail;

            sig = (gpio_all >> info->gpio_sig) & 1;
            new_tail = (info->tail+1) % MAX_SIG;
            info->sum += (sig - info->sig[new_tail]);
            info->sig[new_tail] = sig;
            info->tail = new_tail;
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