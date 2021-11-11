#include <common.h>

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

    // ---------------------------------
    // ---- color organ code folows ----
    // ---------------------------------

    double low_cal, mid_cal, high_cal;
    double low, mid, high;
    bool precal;
    int cnt = 0;

    // xxx comment
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

        // xxx explain this
        if (!precal) {
            bool flag = false;
            if (low > low_cal) { low_cal = low; flag = true; } else { low_cal *= .9999; }
            if (mid > mid_cal) { mid_cal = mid; flag = true; } else { mid_cal *= .9999; }
            if (high > high_cal) { high_cal = high; flag = true; } else { high_cal *= .9999; }
            if (flag) INFO("AUTOCAL: %0.1f %0.1f %0.1f\n", low_cal, mid_cal, high_cal);
        } else {
            if (low > low_cal || mid > mid_cal || high > high_cal) {
                INFO("PRECAL: %0.1f %0.1f %0.1f\n", low_cal, mid_cal, high_cal);
            }
        }

        // set the leds, based on the sound intensity and the calibration values
        #define MAX_BRIGHTNESS 60
        for (int i = 0; i < 4; i++) {
            leds_stage_led(i+0, LED_RED,   low * (MAX_BRIGHTNESS / low_cal));
            leds_stage_led(i+4, LED_GREEN, mid * (MAX_BRIGHTNESS / mid_cal));
            leds_stage_led(i+8, LED_BLUE, high * (MAX_BRIGHTNESS / high_cal));
        }
        leds_commit();
    }

    // success
    return 0;
}
