#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include <body.h>
#include <proximity.h>
#include <misc.h>

#define MAX_PROXIMITY 2

bool sig_rcvd;

void sig_hndlr(int sig)
{
    sig_rcvd = true;
}

int main(int argc, char **argv)
{
    int id, rc, count=0;
    struct sigaction act;

    memset(&act, 0, sizeof(act));
    act.sa_handler = sig_hndlr;
    sigaction(SIGINT, &act, NULL);

    printf("calling proximity_init\n");
    rc = proximity_init(MAX_PROXIMITY, 
                        PROXIMITY_FRONT_GPIO_SIG, PROXIMITY_FRONT_GPIO_ENABLE,
                        PROXIMITY_REAR_GPIO_SIG,  PROXIMITY_REAR_GPIO_ENABLE);
    if (rc < 0) {
        printf("ERROR: proximity_init failed\n");
        return 1;
    }

    printf("sleep 10 secs\n");
    sleep(10);

    printf("enabling proximity sensors\n\n");
    for (id = 0; id < MAX_PROXIMITY; id++) {
        proximity_enable(id);
    }

    printf("polling proximity sensors ...\n");
    while (!sig_rcvd) {
        // print poll_intvl_us every 10 seconds
        if ((++count % 1000) == 0) {
            int poll_intvl_us;
            proximity_get_poll_intvl_us(&poll_intvl_us);
            printf("poll interval = %d us\n", poll_intvl_us);
        }

        // check both sensors for proximity alert, and
        // print when detected
        for (id = 0; id < MAX_PROXIMITY; id++) {
            bool alert;
            double sig;

            proximity_check(id, &alert, &sig);
            if (alert) {
                printf("ALERT %d, sig=%0.2f\a\n", id, sig);
            }
        }

        // sleep 10 ms
        usleep(10000);
    }

    printf("\n\ndisabling proximity sensors\n");
    for (id = 0; id < MAX_PROXIMITY; id++) {
        proximity_disable(id);
    }

    return 0;
}
