// NOTES:
//   ps -eLo comm,rtprio
//   ps -eLo pid,comm,rtprio,sched

#define _GNU_SOURCE

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#include <pthread.h>
#include <sched.h>
#include <time.h>

#include <gpio.h>
#include <timer.h>

#define MAX_HISTOGRAM 100000

int histogram[MAX_HISTOGRAM];
int histogram_overflow;

void *test_thread(void *cx);
void add_histogram(int us);
void sig_hndlr(int);

// ------------------------------------------------------------

int main(int argc, char **argv)
{
    pthread_t tid;
    struct sigaction act;

    // init
    act.sa_handler = sig_hndlr;
    sigaction(SIGINT, &act, NULL);
    setlinebuf(stdout);
    timer_init();
    gpio_init(true);
    set_gpio_func(26, FUNC_OUT);

    // create test thread
    pthread_create(&tid, NULL, test_thread, NULL);

    // pause
    pause();

    // print results
    printf("\nhistogram_overflow = %d\n", histogram_overflow);
    printf("\n");
    for (int us = 0; us < MAX_HISTOGRAM; us++) {
        if (histogram[us]) {
            printf("%5d us : %6d cnt\n", us, histogram[us]);
        }
    }

    // done
    return 0;
}

// This test is trying to create a square wave with a 2 us period.
// However, the result is a square wave with 18 us period, and some jitter 
//  observed on oscilloscope.
void *test_thread(void *cx)
{
    struct timespec ts;
    struct sched_param param;
    cpu_set_t cpu_set;
    int rc;
    uint64_t t_now, t_prior;

    // set affinity to cpu 2
    printf("setting affinity\n");
    CPU_ZERO(&cpu_set);
    CPU_SET(2, &cpu_set);
    rc = sched_setaffinity(0,sizeof(cpu_set_t),&cpu_set);
    if (rc < 0) {
        printf("ERROR: sched_setaffinity, %s\n", strerror(errno));
        exit(1);
    }

    // set realtime priority
    printf("setting priority\n");
    memset(&param, 0, sizeof(param));
    param.sched_priority = 99;
    rc = sched_setscheduler(0, SCHED_FIFO, &param);
    if (rc < 0) {
        printf("ERROR: sched_setscheduler, %s\n", strerror(errno));
        exit(1);
    }

    // loop, attempting to create a square wave with 2 us period
    printf("create square wave loop\n");
    ts.tv_sec = 0;
    ts.tv_nsec = 1000;
    t_prior = timer_get();
    while (true) {
        // set gpio 26, and 1 usec sleep
        gpio_write(26, 1);
        nanosleep(&ts, NULL);
        t_now = timer_get();
        add_histogram(t_now-t_prior);
        t_prior = t_now;

        // clear gpio 26, and 1 usec sleep
        gpio_write(26, 0);
        nanosleep(&ts, NULL);
        t_now = timer_get();
        add_histogram(t_now-t_prior);
        t_prior = t_now;
    }

    return NULL;
}

void add_histogram(int us)
{
    if (us < 0) {
        printf("FATAL us=%d\n", us);
        exit(1);
    }

    if (us >= MAX_HISTOGRAM) {
        histogram_overflow++;
    } else {
        histogram[us]++;
    }
}

void sig_hndlr(int signum)
{
}

