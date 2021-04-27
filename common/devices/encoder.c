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

#define MAX_HISTORY 1024

//
// variables
//

static int encoder_tbl[][4] =
    { {  0, -1, +1,  2 },
      { +1,  0,  2, -1 },
      { -1,  2,  0, +1 },
      {  2, +1, -1,  0 } };

static struct info_s {
    int  gpio_a;
    int  gpio_b;
    bool enabled;
    bool was_disabled;
    int  last_val;
    int  pos;
    int  pos_offset;
    int  errors;
    struct history_s {
        int pos;
        uint64_t time;
    } history[MAX_HISTORY];
    int history_tail;
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

void encoder_enable(int id)
{
    info_tbl[id].enabled = true;
}

void encoder_disable(int id)
{
    info_tbl[id].enabled = false;
    info_tbl[id].was_disabled = true;
}

void encoder_pos_reset(int id)
{
    info_tbl[id].pos_offset = info_tbl[id].pos;
}    

bool encoder_get_enabled(int id)
{
    return info_tbl[id].enabled;
}

int encoder_get_position(int id)
{
    return info_tbl[id].pos - info_tbl[id].pos_offset;
}

int encoder_get_speed(int id)
{
    struct info_s *info = &info_tbl[id];
    uint64_t time_now = timer_get();
    struct history_s h;
    int64_t delta_t;
    int speed;

    h = info->history[ (info->history_tail+50) % MAX_HISTORY ];
    if (h.time == 0) {
        return 0;
    }

    delta_t = time_now - h.time;
    if (delta_t > 100000) {
        return 0;
    }

    speed = 1000000LL * (info->pos - h.pos) / delta_t;
    return speed;
}

int encoder_get_errors(int id)
{
    return info_tbl[id].errors;
}

int encoder_get_poll_intvl_us(void)
{
    int lcl_poll_rate = poll_rate;

    return (lcl_poll_rate > 0 ? 1000000 / lcl_poll_rate : -1);
}

// -----------------  ENCODER THREAD  -----------------------------

static void *encoder_thread(void *cx)
{
    int val, x, rc, id;
    unsigned int gpio_all;
    struct timespec ts;
    struct sched_param param;
    cpu_set_t cpu_set;
    uint64_t time_now;

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
            poll_count_t_last = timer_get();
            poll_count = 0;
            usleep(10000);  // 10 ms
            continue;
        }

        // read all gpio pins, and get the time_now
        gpio_all = gpio_read_all();
        time_now = timer_get();

        // loop over encoders
        for (id = 0; id < max_info; id++) {
            struct info_s * info = &info_tbl[id];

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
            if (x == 2) {
                info->errors++;
            } else {
                info->pos += x;
            }

            // save history of position values, used to determine speed
            int tail = info->history_tail;
            info->history[tail].pos = info->pos;
            info->history[tail].time = time_now;
            __sync_synchronize();
            info->history_tail = (tail + 1) % MAX_HISTORY;
        }

        // this is used to determine the frequency of this code, which 
        // should ideally be 100000 per second when the sleep below is 10 us;
        // however the measured poll_rate is about 47000 per sec
        poll_count++;
        if ((poll_count & 0xfff) == 0) {
            if (time_now > poll_count_t_last + 1000000) {
                poll_rate = 1000000LL * poll_count / (time_now - poll_count_t_last);
                poll_count_t_last = time_now;
                poll_count = 0;
            }
        }

        // sleep for 10 us xxx or 20
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
