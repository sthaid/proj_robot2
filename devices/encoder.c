#define _GNU_SOURCE

#include <stdint.h>
#include <pthread.h>
#include <sched.h>
#include <string.h>
#include <errno.h>

#include <config_hw.h>
#include <encoder.h>
#include <gpio.h>
#include <timer.h>
#include <misc.h>

// notes:
// - 32 bit position var overflow after 69 hours full speed:
//      980 cnt/rev * 500 rpm / 60 ~=  8000 count/sec
//      2e9 / 8000 / 3600 = 69 hours
// - encoder_tbl is from
//   https://cdn.sparkfun.com/datasheets/Robotics/How%20to%20use%20a%20quadrature%20encoder.pdf

//
// defines
//

#define MAX_ENCODER (sizeof(info_tbl) / sizeof(struct info_s))
#define MAX_HISTORY 1024  // suggest using pwr of 2

//
// variables
//

static int encoder_tbl[][4] =
    { {  0, -1, +1,  2 },
      { +1,  0,  2, -1 },
      { -1,  2,  0, +1 },
      {  2, +1, -1,  0 } };

static struct info_s {
    int gpio_a;
    int gpio_b;
    int errors;
    int tail;
    struct history_s {
        int pos;
        uint64_t time;
    } history[MAX_HISTORY];  // xxx 256
} info_tbl[] = {
    { ENCODER0_GPIO_A, ENCODER0_GPIO_B, },
                };

static int poll_count;  // XXX display this in another thread for debug

//
// prototypes
//

static void *encoder_thread(void *cx);

// -----------------  API  ----------------------------------------

int encoder_init(void)
{
    static pthread_t tid;

    // sanity check that encoder_init has not already been called
    if (tid) {
        ERROR("already initialized\n");
        return -1;
    }

    // create the thread to process the encoder gpio values, and to
    // keep track of accumulated shaft position
    pthread_create(&tid, NULL,*encoder_thread, NULL);

    // success
    return 0;
}

// XXX comments
void encoder_get(int id, int *position, int *speed)
{
    #define HIST(n)  (hist[(n) % MAX_HISTORY])

    if (id < 0 || id >= MAX_ENCODER) {
        FATAL("invalid id %d\n", id);
    }

    struct info_s * info = &info_tbl[id];
    struct history_s * hist = info->history;
    int tail, start, end, idx;
    uint64_t time_now, time_desired;

    time_now = timer_get();
    time_desired = time_now - 20000;   // 20 ms earlier than now

    tail = info->tail;
    end = tail;
    start = end - 200;
    if (start < 0) start = 0;

    // XXX put in a safety net
    while (true) {
        if (start == end) {
            idx = start;
            break;
        }
        idx = (start + end) / 2;
        if ((HIST(idx).time < time_desired) && (HIST(idx+1).time >= time_desired)) {
            break;
        } else if (HIST(idx).time < time_desired) {
            start = idx + 1;
        } else {
            end = idx - 1;
            if (end < start) {
                end = start;
            }
        }
    }

    *speed = (int64_t)1000000 * (HIST(tail).pos - HIST(idx).pos) / 
             (signed)(time_now - HIST(idx).time);
    *position = HIST(tail).pos;
}

// -----------------  ENCODER THREAD  -----------------------------

static void *encoder_thread(void *cx)
{
    int val, last_val[MAX_ENCODER], x, rc, id;
    unsigned int gpio_all;
    struct timespec ts = {0,1000};  // 1 us
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
    param.sched_priority = 99;
    rc = sched_setscheduler(0, SCHED_FIFO, &param);
    if (rc < 0) {
        FATAL("sched_setscheduler, %s\n", strerror(errno));
    }

    // XXX comment
    // loop, reading the encode gpio, and updating:
    // - position:    accumulated shaft position, this can be reset to 0 via ctrl+backslash
    // - error_count: number of out of sequence encoder gpio values
    // - poll_count:  number of times the gpio values have been read,
    //                this gets reset to 0 every second in main()
    gpio_all = gpio_read_all();
    for (id = 0; id < MAX_ENCODER; id++) {
        struct info_s * info = &info_tbl[id];
        last_val[id] = (IS_BIT_SET(gpio_all,info->gpio_a) << 1) | IS_BIT_SET(gpio_all,info->gpio_b);
    }

    while (true) {
        gpio_all = gpio_read_all();

        for (id = 0; id < MAX_ENCODER; id++) {
            struct info_s * info = &info_tbl[id];
            struct history_s * hist = info->history;

            val = (IS_BIT_SET(gpio_all,info->gpio_a) << 1) | IS_BIT_SET(gpio_all,info->gpio_b);
            x = encoder_tbl[last_val[id]][val];
            last_val[id] = val;

            if (x == 0) {
                continue;
            } else if (x == 2) {
                info->errors++;
                WARN("encoder sequence error on id=%d\n", id);
            } else {
                int tail = info->tail;
                int new_tail = tail + 1;
                HIST(new_tail).pos = HIST(tail).pos + x;
                HIST(new_tail).time = timer_get();
                __sync_synchronize();
                info->tail = new_tail;
            }
        }

        poll_count++;

        nanosleep(&ts, NULL);
    }

    return NULL;
}
