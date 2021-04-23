#include "common.h"

//
// defines
//

#define DEFAULT_OLED_ADVANCE_INTVL_MS  2000  // 2 secs

//
// variables
//

static char oled_strs[MAX_OLED_STR][MAX_OLED_STR_SIZE];
static int  oled_stridx;
static int  oled_advance_intvl_ms = DEFAULT_OLED_ADVANCE_INTVL_MS;
static bool oled_button_advance_req;

//
// prototypes
//

static void *oled_ctlr_thread(void *cx);
static void oled_button_cb(int id, bool pressed, int pressed_duration_us);

// -----------------  API ROUTINES  -------------------------------------------

int oled_ctlr_init(void)
{
    pthread_t tid;

    // create oled_ctlr_thread
    pthread_create(&tid, NULL, oled_ctlr_thread, NULL);

    // register for right button press callback
    button_register_cb(1, oled_button_cb);

    return 0;
}

oled_strs_t *oled_get_strs(void)
{
    return &oled_strs;
}

// -----------------  PRIVATE ROUTINES  ---------------------------------------

static void *oled_ctlr_thread(void *cx)
{
    int   count=0, count_last_oled_advance=0;
    char *str_to_display;
    char  str_currently_displayed[MAX_OLED_STR_SIZE] = "";

    while (true) {
        // update oled_str array once per second
        if ((count % 10) == 0) {
            mc_status_t *mcs = mc_get_status();
            double electronics_current = current_read_smoothed(0);

            if (MAX_OLED_STR != 5) {
                FATAL("MAX_OLED_STR\n");
            }

            snprintf(oled_strs[0], MAX_OLED_STR_SIZE,
                     "V=%-5.2f", mcs->voltage);
            snprintf(oled_strs[1], MAX_OLED_STR_SIZE,
                     "I=%-4.2f", electronics_current + mcs->motors_current);
            snprintf(oled_strs[2], MAX_OLED_STR_SIZE,
                     "H=%-3.0f", imu_read_magnetometer());
            snprintf(oled_strs[3], MAX_OLED_STR_SIZE,
                     "T=%-4.1f", env_read_temperature_degf());
            snprintf(oled_strs[4], MAX_OLED_STR_SIZE,
                     "P=%-5.2f", env_read_pressure_inhg());
        }

        // check if should display the next str
        if ((oled_advance_intvl_ms != 0 && (count-count_last_oled_advance)*100 > oled_advance_intvl_ms) ||
            (oled_button_advance_req))
        {
            oled_stridx = (oled_stridx + 1) % MAX_OLED_STR;
            count_last_oled_advance = count;
            oled_button_advance_req = false;
        }

        // if oled string that is to be displayed differs from last displayed
        // then display the new string
        str_to_display = oled_strs[oled_stridx];
        if (strcmp(str_to_display, str_currently_displayed) != 0) {
            oled_draw_str(0, str_to_display);
            strcpy(str_currently_displayed, str_to_display);
        }

        // sleep 100 ms
        usleep(100000);
        count++;
    }

    return NULL;
}

static void oled_button_cb(int id, bool pressed, int pressed_duration_us)
{
    if (pressed) {
        // button pressed
        if (oled_advance_intvl_ms > 0) {
            oled_advance_intvl_ms = 0;
        } else {
            oled_button_advance_req = true;
        }
    } else if (pressed_duration_us > 1000000) {
        // button released, and was held for greater than 1 secs
        oled_advance_intvl_ms = DEFAULT_OLED_ADVANCE_INTVL_MS;
    }
}
