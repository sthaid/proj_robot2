#include "common.h"

//
// defines
//

//
// variables
//

static bool emer_stop_sigint;
static bool emer_stop_button;

//
// prototypes
//

static void emer_stop_sigint_hndlr(int sig);
static void emer_stop_button_cb(int id, bool pressed, int pressed_duration_us);
static void *emer_stop_thread(void *cx);  //xxx where should this live

// -----------------  API  --------------------------------------------------

int drive_init(void)
{
    pthread_t tid;
    struct sigaction act;

    // register the emer_stop_sigint_hndlr
    memset(&act, 0, sizeof(act));
    act.sa_handler = emer_stop_sigint_hndlr;
    sigaction(SIGINT, &act, NULL);

    // regiseter for emer_stop_button callback
    button_register_cb(0, emer_stop_button_cb);
    
    // create the emer_stop_thread
    pthread_create(&tid, NULL, emer_stop_thread, NULL);

    // success
    return 0;
}

void drive_go(int left_speed, int right_speed)
{
    encoder_reset(0);
    encoder_reset(1);
    encoder_enable(0);  // XXX maybe the monito thread shuld do all this
    encoder_enable(1);

    mc_enable_all();

    mc_set_speed_all(left_speed, right_speed);
}

void drive_stop(void)
{
    mc_set_speed_all(0,0);

    //encoder_disable(0);  XXX not fully stopped yet
    //encoder_disable(1);
}

// -----------------  EMERGENCY STOP THREAD  -------------------------------

static void *emer_stop_thread(void *cx)
{
    while (true) {
        if (emer_stop_sigint) {
            mc_emergency_stop_all("ctrl-c");
            emer_stop_sigint = false;

            encoder_disable(0);  // xxx in one common place
            encoder_disable(1);
        }
        if (emer_stop_button) {
            mc_emergency_stop_all("button");
            emer_stop_button = false;

            encoder_disable(0);  // xxx in one common place
            encoder_disable(1);
        }

        // XXX add other checks in here too

        usleep(10000);
    }
    return NULL;
}

static void emer_stop_button_cb(int id, bool pressed, int pressed_duration_us)
{
    if (pressed) {
        emer_stop_button = true;
    }
}

static void emer_stop_sigint_hndlr(int sig)
{
    emer_stop_sigint = true;
}


// xxxxxxxxxxxxxxxxx later xxxxxxxxxxxxxxxx

#if 0 // xxx
    // enable capabilities
    proximity_enable(0);   // front
    proximity_enable(1);   // rear
#endif

