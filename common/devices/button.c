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

// defines

#define MAX_EVLIST 4

// variables

static struct info_s {
    int gpio;
    bool last_state;
    uint64_t pressed_time_us;
} info_tbl[10];
static int max_info;

static button_event_t evlist[MAX_EVLIST];
static int evlist_head;
static int evlist_tail;

// prototypes

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

void button_get_current_state(int id, int *state)
{
    // return the current state of the specified button
    if (gpio_read(info_tbl[id].gpio)) {
        *state = BUTTON_STATE_RELEASED;
    } else {
        *state = BUTTON_STATE_PRESSED;
    }
}

int button_get_event(button_event_t *ev)
{
    // if no events in list then return -1
    if (evlist_head == evlist_tail) {
        memset(ev, 0, sizeof(button_event_t));
        return -1;
    }

    // return the oldest event
    *ev = evlist[evlist_head];
    evlist_head = (evlist_head + 1) % MAX_EVLIST;
    return 0;
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

        // loop over defined buttons, and determine if the button has 
        // just been pressed or released
        for (id = 0; id < max_info; id++) {
            struct info_s *x = &info_tbl[id];
            bool curr_state;

            curr_state = IS_BIT_CLR(gpio_all, x->gpio);
            if (curr_state != x->last_state) {
                if ((evlist_tail+1) % MAX_EVLIST == evlist_head) {
                    ERROR("button evlist is full\n");
                } else {
                    button_event_t *ev = &evlist[evlist_tail];
                    ev->id = id;
                    ev->state = (curr_state ? BUTTON_STATE_PRESSED : BUTTON_STATE_RELEASED);
                    ev->pressed_duration_us = 0;
                    if (ev->state == BUTTON_STATE_PRESSED) {
                        x->pressed_time_us = microsec_timer();
                    } else {
                        if (x->pressed_time_us != 0) {
                            ev->pressed_duration_us = microsec_timer() - x->pressed_time_us;
                            x->pressed_time_us = 0;
                        }
                    }
                    __sync_synchronize();
                    evlist_tail = (evlist_tail+1) % MAX_EVLIST;
                }
            }
            x->last_state = curr_state;
        }

        // sleep for 10 ms
        usleep(10000);
    }

    return NULL;
}
