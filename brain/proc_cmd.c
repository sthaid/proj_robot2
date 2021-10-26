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

static void hndlr_set_volume(args_t args);
static void hndlr_get_volume(args_t args);
static void hndlr_sleep(args_t args);
static void hndlr_end_program(args_t args);
static void hndlr_time(args_t args);
static void hndlr_set_info(args_t args);
static void hndlr_get_info(args_t args);
static void hndlr_drive_fwd(args_t args);

#define HNDLR(name) { #name, hndlr_##name }

static hndlr_lookup_t hndlr_lookup_tbl[] = {
    HNDLR(set_volume),
    HNDLR(get_volume),
    HNDLR(sleep),
    HNDLR(end_program),
    HNDLR(time),
    HNDLR(set_info),
    HNDLR(get_info),
    HNDLR(drive_fwd),
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

bool proc_cmd_in_progress(void)
{
    return cmd != NULL;
}

void proc_cmd_cancel(void)
{
    cancel = true;
}

// -----------------  CMD THREAD  -------------------------------------------

static void *cmd_thread(void *cx)
{
    hndlr_t proc;
    args_t args;

    while (true) {
        // wait for cmd
        while (cmd == NULL) {
            usleep(10000);
        }

        // check if cmd matches known grammar, and call hndlr proc
        bool match = grammar_match(cmd, &proc, args);
        INFO("match=%d, args=  '%s'  '%s'  '%s'  '%s'\n", match, args[0], args[1], args[2], args[3]);
        if (match) {
            proc(args);
            if (cancel) audio_out_beep(6);
        } else {
            audio_out_beep(3);
        }

        // done with this cmd
        free(cmd);
        cmd = NULL;
        cancel = false;
    }

    return NULL;
}

// -----------------  PROC CMD HANDLERS  ------------------------------------

static void hndlr_set_volume(args_t args)
{
    char *action = args[0];
    char *amount = args[1];

    if (strmatch(action, "up", "increase", NULL)) {
        t2s_set_volume(getnum(amount, DELTA_VOLUME), true);
    } else if (strmatch(action, "down", "decrease", NULL)) {
        t2s_set_volume(-getnum(amount, DELTA_VOLUME), true);
    } else if (strmatch(action, "set", NULL)) {
        t2s_set_volume(getnum(amount, DEFAULT_VOLUME), false);
    } else if (strmatch(action, "reset", NULL)) {
        t2s_set_volume(DEFAULT_VOLUME, false);
    }
    t2s_play("The volume is now %d%%.", t2s_get_volume());
}

static void hndlr_get_volume(args_t args)
{
    t2s_play("The volume is %d%%.", t2s_get_volume());
}

static void hndlr_sleep(args_t args)
{
    int secs = getnum(args[0], 10);
    
    t2s_play("Sleeping for %d seconds.", secs);

    for (int i = 0; i < 10*secs; i++) {
        usleep(100000);
        if (cancel) break;
    }
}

static void hndlr_end_program(args_t args)
{
    brain_end_program();
}

static void hndlr_time(args_t args)
{
    struct tm *tm;
    time_t t = time(NULL);

    tm = localtime(&t);
    t2s_play_nodb("the time is %d:%d", tm->tm_hour, tm->tm_min);
}

static void hndlr_set_info(args_t args)
{
    char *info_id = args[0];
    char *info_val = args[1];
    INFO("XXX '%s' = '%s'\n", info_id, info_val);

    t2s_play("your %s is %s, got it!", info_id, info_val);
    db_set(KEYID_INFO, info_id, info_val, strlen(info_val)+1);
}

static void hndlr_get_info(args_t args)
{
    char *info_id = args[0];
    char *info_val;
    unsigned int info_val_len;

    if (db_get(KEYID_INFO, args[0], (void**)&info_val, &info_val_len) < 0) {
        t2s_play("I don't know your %s", info_id);
    } else {
        t2s_play("your %s is %s", info_id, info_val);
    }
}

// xxx define for 11
// xxx return status
static void hndlr_drive_fwd(args_t args)
{
    int feet = getnum(args[0], 0);
    char failure_reason[200];
    int rc;

    rc = body_drive_cmd(11, feet, 0, 0, 0, failure_reason);
        
    if (rc < 0) {
        t2s_play("%s", failure_reason);
    }
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
