#include <common.h>

//
// variables
//

static char *cmd;
static bool cancel;

//
// prototypes
//

static void *cmd_thread(void *cx);
static int getnum(char *s, int default_value);
static bool strmatch(char *s, ...);

//
// handlers
//

// program control & test
static int hndlr_end_program(args_t args);
static int hndlr_restart_program(args_t args);
static int hndlr_reset_mic(args_t args);
static int hndlr_playback(args_t args);
// volume control
static int hndlr_set_volume(args_t args);
static int hndlr_get_volume(args_t args);
// personal info
static int hndlr_set_info(args_t args);
static int hndlr_get_info(args_t args);
// body status & power
static int hndlr_body_power(args_t args);
static int hndlr_status_report(args_t args);
// body drive
static int hndlr_drive_fwd(args_t args);
static int hndlr_drive_rotate(args_t args);
// misc
static int hndlr_time(args_t args);
static int hndlr_weather_report(args_t args);
static int hndlr_count(args_t args);
static int hndlr_polite_conversation(args_t args);
static int hndlr_lights(args_t args);
static int hndlr_play_music(args_t args);
static int play_music_file(char *filename);

#define HNDLR(name) { #name, hndlr_##name }

static hndlr_lookup_t hndlr_lookup_tbl[] = {
    // program control & test
    HNDLR(end_program),
    HNDLR(restart_program),
    HNDLR(reset_mic),
    HNDLR(playback),
    // volume control
    HNDLR(set_volume),
    HNDLR(get_volume),
    // personal info
    HNDLR(set_info),
    HNDLR(get_info),
    // body status & power
    HNDLR(body_power),
    HNDLR(status_report),
    // body drive
    HNDLR(drive_fwd),
    HNDLR(drive_rotate),
    // misc
    HNDLR(time),
    HNDLR(weather_report),
    HNDLR(count),
    HNDLR(polite_conversation),
    HNDLR(lights),
    HNDLR(play_music),
    { NULL, NULL }
                };

// -----------------  INIT  -------------------------------------------------

void proc_cmd_init(void)
{
    pthread_t tid;

    grammar_init("grammar", hndlr_lookup_tbl);
    pthread_create(&tid, NULL, cmd_thread, NULL);
}

// -----------------  RUNTIME API  ------------------------------------------

void proc_cmd_execute(char *transcript, double doa)
{
    cmd = transcript;
}

bool proc_cmd_in_progress(bool *succ)
{
    if (cmd != NULL && cmd != (void*)1) {
        // in progress
        *succ = false;
        return true;
    } else {
        // completed
        *succ = (cmd == NULL);
        return false;
    }
}

void proc_cmd_cancel(void)
{
    INFO("CANCEL CMD\n");
    cancel = true;
    audio_out_cancel();
    body_emer_stop();
}

// -----------------  CMD THREAD  -------------------------------------------

static void *cmd_thread(void *cx)
{
    hndlr_t proc;
    args_t args;
    int rc;

    while (true) {
        // wait for cmd
        while (cmd == NULL || cmd == (void*)1) {
            usleep(10000);
        }

        // check if cmd matches known grammar, and call hndlr proc
        bool match = grammar_match(cmd, &proc, args);
        INFO("match=%d, args=  '%s'  '%s'  '%s'  '%s'\n", match, args[0], args[1], args[2], args[3]);
        if (match) {
            cancel = false;
            rc = proc(args);
            if (cancel) rc = -1;
        } else {
            audio_out_beep(2);
            rc = -1;
        }

        // wait for audio output to complete
        audio_out_wait();

        // done with this cmd
        free(cmd);
        cmd = (rc == 0 ? NULL : (void*)1);
    }

    return NULL;
}

// -----------------  PROC CMD HANDLERS  ------------------------------------

// ----------------------
// program control & test
// ----------------------

static int hndlr_end_program(args_t args)
{
    brain_end_program();

    return 0;
}

static int hndlr_restart_program(args_t args)
{
    brain_restart_program();

    return 0;
}

static int hndlr_reset_mic(args_t args)
{
    int rc = audio_in_reset_mic();
    if (rc < 0) {
        t2s_play("failed to reset microphone");
        return -1;
    } else {
        t2s_play("the microphone has been reset");
        return 0;
    }
}

static int hndlr_playback(args_t args)
{
    #define MAX_DATA (5*16000)
    short data[MAX_DATA];

    brain_get_recording(data, MAX_DATA);
    audio_out_play_data(data, MAX_DATA, 16000);
    return 0;
}
// ----------------------
// volume control
// ----------------------

static int hndlr_set_volume(args_t args)
{
    char *action = args[0];
    char *amount = args[1];

    if (strmatch(action, "up", "increase", NULL)) {
        audio_out_set_volume(getnum(amount, DELTA_VOLUME), true);
    } else if (strmatch(action, "down", "decrease", NULL)) {
        audio_out_set_volume(-getnum(amount, DELTA_VOLUME), true);
    } else if (strmatch(action, "set", NULL)) {
        audio_out_set_volume(getnum(amount, DEFAULT_VOLUME), false);
    } else if (strmatch(action, "reset", NULL)) {
        audio_out_set_volume(DEFAULT_VOLUME, false);
    } else {
        ERROR("hndllr_se_volume unexpected action '%s'\n", action);
        return -1;
    }

    t2s_play("The volume is now %d%%.", audio_out_get_volume());

    return 0;
}

static int hndlr_get_volume(args_t args)
{
    t2s_play("The volume is %d%%.", audio_out_get_volume());

    return 0;
}

// ----------------------
// personal info
// ----------------------

static int hndlr_set_info(args_t args)
{
    char *info_id = args[0];
    char *info_val = args[1];

    t2s_play("your %s is %s, got it!", info_id, info_val);
    db_set(KEYID_INFO, info_id, info_val, strlen(info_val)+1);

    return 0;
}

static int hndlr_get_info(args_t args)
{
    char *info_id = args[0];
    char *info_val;
    unsigned int info_val_len;
    int rc;

    if (db_get(KEYID_INFO, args[0], (void**)&info_val, &info_val_len) < 0) {
        t2s_play("I don't know your %s", info_id);
        rc = -1;
    } else {
        t2s_play("your %s is %s", info_id, info_val);
        rc = 0;
    }

    return rc;
}

// ----------------------
// body status & power
// ----------------------

static int hndlr_status_report(args_t args)
{
    body_status_report();

    return 0;
}

static int hndlr_body_power(args_t args)
{
    char *onoff = args[0];

    if (strcmp(onoff, "on") == 0) {
        body_power_on();
    } else {
        body_power_off();
    }

    return 0;
}

// ----------------------
// body drive
// ----------------------

static int hndlr_drive_fwd(args_t args)
{
    int feet = getnum(args[0], 0);
    char failure_reason[200];
    int rc;

    // xxx define for 11
    rc = body_drive_cmd(11, feet, 0, 0, 0, failure_reason);
    if (rc < 0) {
        t2s_play("%s", failure_reason);
    }

    return rc;
}

static int hndlr_drive_rotate(args_t args)
{
    char *amount    = args[0];
    char *direction = args[1];
    bool dir_is_clockwise;
    int  degrees;

    dir_is_clockwise = (strcmp(direction, "clockwise") == 0) ||
                       (strcmp(direction, "") == 0);

    if (sscanf(amount, "%d degrees", &degrees) == 1) {
        // okay
    } else if (strcmp(amount, "halfway around") == 0) {
        degrees = 180;
    } else {
        degrees = 360;
    }

    t2s_play("rotate %s %d degrees",
             dir_is_clockwise ? "clockwise" : "counterclockwise",
             degrees);

    return 0;
}

// ----------------------
// misc
// ----------------------

static int hndlr_time(args_t args)
{
    struct tm *tm;
    time_t t = time(NULL);

    tm = localtime(&t);
    t2s_play("the time is");  // xxx make same change in other places
    t2s_play_nodb("%d %2.2d", tm->tm_hour, tm->tm_min);

    return 0;
}

static int hndlr_weather_report(args_t args)
{
    body_weather_report();

    return 0;
}

static int hndlr_count(args_t args)
{
    int cnt = getnum(args[0], 10);

    for (int i = 1; i <= cnt; i++) {
        t2s_play("%d", i);
        usleep(200000);
        if (cancel) break;
    }        

    return 0;
}

static int hndlr_polite_conversation(args_t args)
{
    static struct {
        char *cmd;
        char *response;
    } tbl[] = {
        { "hello",
          "Hello to you to" },
        { "how are you",
          "I am well, thank you for asking" },
        { "how do you feel",
          "I am well, thank you for asking" },
        { "how old are you", 
          "I am still very young" },
        { "what is your favorite color",
          "I like purple" },
        { "what is your name",
          "I haven't decided on a name yet" },
                };

    for (int i = 0; i < sizeof(tbl)/sizeof(tbl[0]); i++) {
        if (strcmp(tbl[i].cmd, args[0]) == 0) {
            t2s_play("%s", tbl[i].response);
            return 0;
        }
    }

    ERROR("polite_conversation: '%s'\n", args[0]);
    t2s_play("Sorry, I do not understand you");
    return 0;
}

static int hndlr_lights(args_t args)
{
    char cmd[200];
    int rc;

    sprintf(cmd, "./tplink-smartplug-master/tplink_smartplug.py  -t 192.168.1.191 -c %s", args[0]);
    rc = system(cmd);

    if (rc != 0) {
        ERROR("failed to turn lights %s, rc=0x%x\n", args[0], rc);
        t2s_play("an error occurred when turning lights %s", args[0]);
        return -1;
    }

    return 0;
}

static int hndlr_play_music(args_t args)
{
    int rc = 0;

    INFO("play %s\n", args[0]);

    if (strcmp(args[0], "music") == 0) {
        char *names[100];
        int max_names;

        // get list of files in the music direcotry, and
        // shuffle the list
        rc = get_filenames("music", names, &max_names);
        if (rc < 0) {
            ERROR("play music failed to get filenames\n");
            return -1;
        }
        shuffle(names, sizeof(void*), max_names);
        for (int i = 0; i < max_names; i++) {
            INFO("shuffled music file list - %s\n", names[i]);
        }

        // play the music files 
        for (int i = 0; i < max_names; i++) {
            if (strstr(names[i], ".wav") == NULL) continue;
            if (strstr(names[i], "frequency_sweep") != NULL) continue;
            if (strstr(names[i], "white_noise") != NULL) continue;
            rc = play_music_file(names[i]);
            if (rc < 0 || cancel) break;
        }

        // free names
        for (int i = 0; i < max_names; i++) {
            free(names[i]);
        }
    } else {
        char filename[200], *p;

        // construct filename
        sprintf(filename, "%s.wav", args[0]);
        for (p = filename; *p; p++) {
            if (*p == ' ') *p = '_';
        }

        // play the music file        
        rc = play_music_file(filename);
    }

    return rc;
}

static int play_music_file(char *filename)
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

// -----------------  SUPPORT  ----------------------------------------------

static int getnum(char *s, int default_value)
{
    int n = default_value;
    sscanf(s, "%d", &n);
    return n;
}

static bool strmatch(char *s, ...)
{
    va_list ap;
    bool match = false;
    char *s1;

    va_start(ap, s);
    while ((s1 = va_arg(ap, char*))) {
        if (strcmp(s, s1) == 0) {
            match = true;
            break;
        }
    }
    va_end(ap);

    return match;
}
