#include <common.h>

typedef struct {
    char     filename[200];
    uint64_t start_time;
} playing_t;

static playing_t *now_playing;

static void color_organ_rev1(char *filename);
static void color_organ_rev2(char *filename);

// ---------------------------------------------------------------------------------

int play_music_file(char *filename)
{
    int rc;
    struct stat buf;
    char announce[200], pathname[200], *p;
    static playing_t playing;

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
    audio_out_wait();
    sleep(1);
    audio_out_play_wav(pathname, false);

    // set now_playing to indicate to the play_music_ignore_cancel routine
    // what music is playing, and when it started
    strcpy(playing.filename, filename);
    playing.start_time = microsec_timer();
    __sync_synchronize();
    now_playing = &playing;

    // call color_organ, this will return when then music is done
    switch (settings.color_organ) {
    case 1:
        color_organ_rev1(filename);
        break;
    case 2:
        color_organ_rev2(filename);
        break;
    default:
        ERROR("settngs.color_organ %d is not supported\n", settings.color_organ);
        audio_out_wait();
        audio_out_set_state_idle();
        break;
    }

    // nothing is now_playing
    now_playing = NULL;

    // success
    return 0;
}

// this routine is called by proc_cmd_cancel to check if the cancel is
// due to a spurious Terminator work word detection from the music
//
// record of false cancel indications:
// - song not_my_name.wav was cancelled at time 234.986 seconds
bool play_music_ignore_cancel(void)
{
    static struct {
        char   *filename;
        int64_t ignore_cancel_duration[10];
    } tbl[] = {
        { "not_my_name.wav",               { 235*SECONDS, 78.6*SECONDS, } },
        { "1_bourbon_1_scotch_1_beer.wav", { 324*SECONDS, } },
        { "thick_as_a_brick.wav",          {  44*SECONDS, } },
            };

    playing_t *playing = now_playing;
    int64_t duration;

    if (playing == NULL) {
        return false;
    }

    duration = (microsec_timer() - playing->start_time);
    
    for (int i = 0; i < sizeof(tbl)/sizeof(tbl[0]); i++) {
        if (strcmp(playing->filename, tbl[i].filename) == 0) {
            for (int j = 0; j < 10; j++) {
                if (tbl[i].ignore_cancel_duration[j] == 0) {
                    break;
                }

                int64_t us_low  = tbl[i].ignore_cancel_duration[j] - 3*SECONDS;
                int64_t us_high = tbl[i].ignore_cancel_duration[j] + 3*SECONDS;
                if (duration >= us_low && duration <= us_high) {
                    INFO("play music ignore cancel, duration=%0.3f\n", (double)duration/SECONDS);
                    return true;
                }
            }
        }
    }

    return false;
}
    
// -----------------  COLOR ORGAN REV1  --------------------------------------------

static void color_organ_rev1(char *filename)
{
    double   low_cal, mid_cal, high_cal;
    double   low, mid, high;
    bool     precal;
    int      cnt = 0;
    uint64_t start_time = microsec_timer();
    bool     cancelled;

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
    while (audio_out_is_complete(&cancelled) == false) {
        // update leds at 10 ms interval
        usleep(10*MS);

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
        leds_commit(settings.brightness);
    }

    // if the music audio output was cancelled then print the time into the
    // song that the cancel occurred
    if (cancelled) {
        double secs = (double)(microsec_timer() - start_time) / SECONDS;
        INFO("song %s was cancelled at time %0.3f seconds\n", filename, secs);
    }

    // since the audio output was started with complete_to_idle set false,
    // call audio_out_set_state_idle
    audio_out_set_state_idle();
}

// -----------------  COLOR ORGAN REV2  --------------------------------------------

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
    uint64_t     start_time = microsec_timer();
    bool         cancelled;

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
    while (audio_out_is_complete(&cancelled) == false) {
        // update leds at 10 ms interval
        usleep(10*MS);

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
        for (int i = 0; i < 4; i++) {
            leds_stage_led(i+0, LED_RED,   low * (15 / avg_vals->low));
            leds_stage_led(i+4, LED_GREEN, mid * (15 / avg_vals->mid));
            leds_stage_led(i+8, LED_BLUE, high * (15 / avg_vals->high));
        }
        leds_commit(settings.brightness);
    }

    // if the music audio output was cancelled then print the time into the
    // song that the cancel occurred
    if (cancelled) {
        double secs = (double)(microsec_timer() - start_time) / SECONDS;
        INFO("song %s was cancelled at time %0.3f seconds\n", filename, secs);
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

    // since the audio output was started with complete_to_idle set false,
    // call audio_out_set_state_idle
    audio_out_set_state_idle();
}
