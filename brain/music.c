#include <common.h>

static int color_organ_select = 2;

static void color_organ_rev1(char *filename);
static void color_organ_rev2(char *filename);

// ---------------------------------------------------------------------------------

int play_music_file(char *filename)
{
    int rc;
    struct stat buf;
    char announce[200], pathname[200], *p;

    INFO("play_music_file: filename = %s\n", filename);

    // construct announce string
    strcpy(announce, filename);
    p = strstr(announce, ".wav"); *p = '\0';
    for (p = announce; *p; p++) if (*p == '_') *p = ' ';

    // stat file to ensure it exists
    sprintf(pathname, "music/%s", filename);
    rc = stat(pathname, &buf);
    if (rc < 0) {
        ERROR("pathname %s, %s\n", pathname, strerror(errno));
        t2s_play("song %s does not exist", announce);
        return -1;
    }

    // announce what song is about to play, and play it
    t2s_play("playing %s", announce);
    audio_out_play_wav(pathname, NULL, 0);

    // call color_organ, this will return when then music is done
    if (color_organ_select == 1) {
        color_organ_rev1(filename);
    } else {
        color_organ_rev2(filename);
    }

    // success
    return 0;
}
    
// -----------------  COLOR ORGAN REV1  --------------------------------------------

static void color_organ_rev1(char *filename)
{
    double low_cal, mid_cal, high_cal;
    double low, mid, high;
    bool precal;
    int cnt = 0;

    INFO("starting color_organ_rev1 for %s\n", filename);

    // precalibrated values are used for the white_noise and frequency_sweep files
    if (strcmp(filename, "white_noise.wav") == 0) {
        low_cal  = 2845.6;
        mid_cal  = 38.6;
        high_cal = 9866.3;
        precal   = true;
    } else if (strcmp(filename, "frequency_sweep.wav") == 0) {
        low_cal  = 20857.3;
        mid_cal  = 236.5;
        high_cal = 13083.4;
        precal   = true;
    } else {
        low_cal  = 1;
        mid_cal  = 1;
        high_cal = 1;
        precal   = false;
    }

    // while song is playing, update the leds based on sound intensity
    while (audio_out_is_complete() == false) {
        // update leds at 10 ms interval
        usleep(10000);

        // ignore the first 100 ms because of possible startup sound glitches
        if (cnt++ < 10) {
            continue;
        }

        // get the intensity of sound in the low, mid, and high frequency ranges
        audio_out_get_low_mid_high(&low, &mid, &high);

        // if not using precalibrated values then the calibration values are
        // cpomputed as the song is playing; the calibrated values directly track increases
        // to low,mid,high; and slowly ramp down during quiet intervals
        if (!precal) {
            bool flag = false;
            if (low > low_cal) { low_cal = low; flag = true; } else { low_cal *= .9999; }
            if (mid > mid_cal) { mid_cal = mid; flag = true; } else { mid_cal *= .9999; }
            if (high > high_cal) { high_cal = high; flag = true; } else { high_cal *= .9999; }
            if (flag) INFO("AUTOCAL: %0.1f %0.1f %0.1f\n", low_cal, mid_cal, high_cal);
        }

        // set the leds, based on the sound intensity and the calibration values
        #define MAX_BRIGHTNESS 100
        for (int i = 0; i < 4; i++) {
            leds_stage_led(i+0, LED_RED,   low * (MAX_BRIGHTNESS / low_cal));
            leds_stage_led(i+4, LED_GREEN, mid * (MAX_BRIGHTNESS / mid_cal));
            leds_stage_led(i+8, LED_BLUE, high * (MAX_BRIGHTNESS / high_cal));
        }
        leds_commit();
    }
}

// -----------------  COLOR ORGAN REV2  --------------------------------------------

// xxx pending audio output can accumulate in the averages

typedef struct {
    double low;
    double mid;
    double high;
    double sum_low;
    double sum_mid;
    double sum_high;
    int    n;
} avg_vals_t;

static void color_organ_rev2(char *filename)
{
    avg_vals_t   new_avg_vals, db_avg_vals, *tmp, *avg_vals;
    double       low, mid, high;
    unsigned int tmp_len;

    INFO("starting color_organ_rev2 for %s\n", filename);

    // init new_avg_vals and db_avg_vals to zero
    memset(&new_avg_vals, 0, sizeof(new_avg_vals));
    memset(&db_avg_vals, 0, sizeof(db_avg_vals));

    // get song average values of low,mid,high from db, if they exist
    db_get(KEYID_COLOR_ORGAN, filename, (void**)&tmp, &tmp_len);
    if (tmp) {
        assert(tmp_len == sizeof(avg_vals_t));
        db_avg_vals = *tmp;
        INFO("got db_avg_vals %8d %8.0f %8.0f %8.0f\n", 
             db_avg_vals.n, db_avg_vals.low, db_avg_vals.mid, db_avg_vals.high);
    }

    // while song is playing, update the leds based on sound intensity
    while (audio_out_is_complete() == false) {
        // update leds at 10 ms interval
        usleep(10000);

        // get the intensity of sound in the low, mid, and high frequency ranges
        audio_out_get_low_mid_high(&low, &mid, &high);

        // compute new_avg_vals; these will be saved to db before returining if
        // the new_avg_vals are for a longer period of the song than the db_avg_vals
        new_avg_vals.sum_low += low;
        new_avg_vals.sum_mid += mid;
        new_avg_vals.sum_high += high;
        new_avg_vals.n++;

        new_avg_vals.low = new_avg_vals.sum_low / new_avg_vals.n;
        new_avg_vals.mid = new_avg_vals.sum_mid / new_avg_vals.n;
        new_avg_vals.high = new_avg_vals.sum_high / new_avg_vals.n;

        // select if the new_avg_vals or the avg_vals retrieved from db will be
        // used when setting the leds below
        avg_vals = (new_avg_vals.n > db_avg_vals.n ? &new_avg_vals : &db_avg_vals);

        // if any of the avg_vals is zero then continue
        if (avg_vals->low == 0 || avg_vals->mid == 0 || avg_vals->high == 0) {
            continue;
        }

        // set the leds
        // xxx more work needed to scale up low values, maybe shouldn't be linear func
        for (int i = 0; i < 4; i++) {
            leds_stage_led(i+0, LED_RED,   low * (15 / avg_vals->low));
            leds_stage_led(i+4, LED_GREEN, mid * (15 / avg_vals->mid));
            leds_stage_led(i+8, LED_BLUE, high * (15 / avg_vals->high));
        }
        leds_commit();
    }

    // store avg low,mid,high in db (but only if we have a more complete average)
    INFO("new_avg_vals.n = %d  db_avg_vals.n = %d - %s to db\n", 
         new_avg_vals.n, db_avg_vals.n,
         new_avg_vals.n > db_avg_vals.n ? "writing" : "not writiing");
    if (new_avg_vals.n > db_avg_vals.n) {
        db_set(KEYID_COLOR_ORGAN, filename, &new_avg_vals, sizeof(new_avg_vals));
        INFO("set db_avg_vals %8d %8.0f %8.0f %8.0f\n", 
             new_avg_vals.n, new_avg_vals.low, new_avg_vals.mid, new_avg_vals.high);
    }
}
