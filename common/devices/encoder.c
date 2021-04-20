#define _GNU_SOURCE

#include <stdint.h>
#include <stdarg.h>
#include <pthread.h>
#include <sched.h>
#include <string.h>
#include <errno.h>

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

#define MAX_HISTORY 4096  // suggest using pwr of 2

//
// variables
//

static int encoder_tbl[][4] =
    { {  0, -1, +1,  2 },
      { +1,  0,  2, -1 },
      { -1,  2,  0, +1 },
      {  2, +1, -1,  0 } };

static struct info_s {
    unsigned int gpio_a;
    unsigned int gpio_b;
    bool         enabled;
    bool         was_disabled;
    unsigned int errors;
    unsigned int last_val;
    int          pos_offset;  // xxx 64bit
    unsigned int tail;
    struct history_s {
        int pos;  // xxx 64bit
        uint64_t time;
    } history[MAX_HISTORY];
} info_tbl[10];
static int max_info;

static unsigned int poll_rate;   // units = persec

//
// prototypes
//

static void *encoder_thread(void *cx);
static bool all_disabled(void);

// -----------------  API  ----------------------------------------

int encoder_init(int max_info_arg, ...)  // int gpio_a, int gpio_b, ...
{
    static pthread_t tid;
    va_list ap;

    // if already initialized then return success
    if (tid) {
        return 0;
    }

    // save hardware info, and init non-zero fields of info_tbl
    va_start(ap, max_info_arg);
    for (int i = 0; i < max_info_arg; i++) {
        info_tbl[i].gpio_a = va_arg(ap, int);
        info_tbl[i].gpio_b = va_arg(ap, int);
        info_tbl[i].was_disabled = true;
    }
    max_info = max_info_arg;
    va_end(ap);

    // init gpio and timer functions
    if (gpio_init() < 0) {
        ERROR("gpio_init failed\n");
        return -1;
    }
    if (timer_init() < 0) {
        ERROR("timer_init failed\n");
        return -1;
    }

    // create the thread to process the encoder gpio values, and to
    // keep track of accumulated shaft position
    pthread_create(&tid, NULL,*encoder_thread, NULL);

    // success
    return 0;
}

void encoder_get_position(int id, int *position)
{
    struct info_s    * info = &info_tbl[id];
    struct history_s * hist = info->history;
    unsigned int       tail = info->tail;

    #define HIST(n) (hist[(n) % MAX_HISTORY])   // xxx use inline routine

    *position = HIST(tail).pos - info->pos_offset;
}

void encoder_get_speed(int id, int *speed)
{
    struct info_s    * info = &info_tbl[id];
    struct history_s * hist = info->history;
    unsigned int       tail = info->tail;
    unsigned int       start, end, idx;
    uint64_t           time_now, time_desired;

    // don't try to get the speed if the encoder is not enabled
    if (info->enabled == false) {
        *speed = 0;
        return;
    }

    // find a history entry that is 20 ms earlier than time_now,
    // use binary search; this entry will be used to calculate the speed
    time_now = timer_get();
    time_desired = time_now - 20000;   // 20 ms earlier than now

    end = tail;
    start = (end >= 200 ? end - 200 : 0);

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

    // return speed
    *speed = (int64_t)1000000 * (HIST(tail).pos - HIST(idx).pos) / 
             (signed)(time_now - HIST(idx).time);
}

void encoder_get_errors(int id, int *errors)
{
    *errors = info_tbl[id].errors;
}

void encoder_pos_reset(int id)
{
    struct info_s    * info = &info_tbl[id];
    struct history_s * hist = info->history;
    unsigned int       tail = info->tail;

    info->pos_offset = HIST(tail).pos;
}    

void encoder_enable(int id)
{
    info_tbl[id].enabled = true;
}

void encoder_disable(int id)
{
    info_tbl[id].enabled = false;
    info_tbl[id].was_disabled = true;
}

// debug routine
void encoder_get_poll_intvl_us(int *poll_intvl_us)
{
    int lcl_poll_rate = poll_rate;

    *poll_intvl_us = (lcl_poll_rate > 0 ? 1000000 / lcl_poll_rate : -1);
}

// -----------------  ENCODER THREAD  -----------------------------

static void *encoder_thread(void *cx)
{
    int val, x, rc, id;
    unsigned int gpio_all;
    struct timespec ts;
    struct sched_param param;
    cpu_set_t cpu_set;

    static uint64_t poll_count, poll_count_t_last;

    // used to determine poll_rate stat
    poll_count_t_last = timer_get();

    // set affinity to cpu 3
    CPU_ZERO(&cpu_set);
    CPU_SET(3, &cpu_set);
    rc = sched_setaffinity(0,sizeof(cpu_set_t),&cpu_set);
    if (rc < 0) {
        FATAL("sched_setaffinity, %s\n", strerror(errno));
    }

    // set realtime priority
    memset(&param, 0, sizeof(param));
    param.sched_priority = 95;
    rc = sched_setscheduler(0, SCHED_FIFO, &param);
    if (rc < 0) {
        FATAL("sched_setscheduler, %s\n", strerror(errno));
    }

    // loop forever
    while (true) {
        // if all encoder sensors are disabled then
        // relatively long sleep and continue; 
        // purpose is to save power
        if (all_disabled()) {
            poll_rate = 0;
            usleep(10000);  // 10 ms
            continue;
        }

        // read all gpio pins
        gpio_all = gpio_read_all();

        // loop over encoders
        for (id = 0; id < max_info; id++) {
            struct info_s * info = &info_tbl[id];
            struct history_s * hist = info->history;

            // if this encoder was_disabled then get it's last value
            if (info->was_disabled) {
                info->last_val = (IS_BIT_SET(gpio_all,info->gpio_a) << 1) | 
                                  IS_BIT_SET(gpio_all,info->gpio_b);
                info->was_disabled = false;
            }

            // determine whether encoder indicates one of the following:
            // - x == 0         no change
            // - x == 1 or -1   increment or decrement
            // - x == 2         error, encoder bits out of sequence
            //                  probably because the encoder values were not read quickly enough
            val = (IS_BIT_SET(gpio_all,info->gpio_a) << 1) | IS_BIT_SET(gpio_all,info->gpio_b);
            x = encoder_tbl[info->last_val][val];
            info->last_val = val;

            // process the 'x'
            if (x == 0) {
                continue;
            } else if (x == 2) {
                info->errors++;
                WARN("encoder %d sequence error\n", id);  // xxx comment this out
            } else {
                // add entry to history array for this encoder, the
                // encoder's position and current time are added; 
                // this information is used by encoder_get() to determine the speed
                unsigned int tail = info->tail;
                unsigned int new_tail = tail + 1;
                HIST(new_tail).pos = HIST(tail).pos + x;
                HIST(new_tail).time = timer_get();
                __sync_synchronize();
                info->tail = new_tail;
            }
        }

        // this is used to determine the frequency of this code, which 
        // should ideally be 100000 per second when the sleep below is 10 us;
        // however the measured poll_rate is about 47000 per sec
        poll_count++;
        if ((poll_count & 0xfff) == 0) {
            uint64_t t_now = timer_get();
            if (t_now > poll_count_t_last + 1000000) {
                poll_rate = 1000000LL * poll_count / (t_now - poll_count_t_last);
                poll_count_t_last = t_now;
                poll_count = 0;
            }
        }

        // sleep for 10 us
        ts.tv_sec = 0;
        ts.tv_nsec = 10000;  //  10 us
        nanosleep(&ts, NULL);
    }

    return NULL;
}

static bool all_disabled(void)
{
    for (int id = 0; id < max_info; id++) {
        struct info_s *info = &info_tbl[id];
        if (info->enabled == true) {
            return false;
        }
    }
    return true;
}
