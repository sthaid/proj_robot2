#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include <config_hw.h>
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
    int id, sum, poll_rate, rc;
    bool sense;
    struct sigaction act;
    bool alert;
    char *p;
    char str[200];

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

    printf("enabling proximity sensors\n\n");
    for (id = 0; id < MAX_PROXIMITY; id++) {
        proximity_enable(id);
    }

    printf("polling proximity sensors ...\n");
    while (!sig_rcvd) {
        p = str;
        alert = false;
        for (id = 0; id < MAX_PROXIMITY; id++) {
            sense = proximity_check(id, &sum, &poll_rate);
            if (sense) alert = true;
            p += sprintf(p, "%9s-%02d -- ",
                    sense ? "   DETECT" : "UN_DETECT",
                    sum);
        }
        sprintf(p, "POLL_RATE %d /sec %s",
                poll_rate,
                alert ? "\a" : "");
        printf("%s\n", str);
        usleep(100000);
    }

    printf("\n\ndisabling proximity sensors\n");
    for (id = 0; id < MAX_PROXIMITY; id++) {
        proximity_disable(id);
    }

    return 0;
}
