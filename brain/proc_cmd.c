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
static bool strmatch(char *s, ...);  // xxx utils

//
// handlers
//

static int hndlr_volume(args_t args);
static int hndlr_sleep(args_t args);

static hndlr_lookup_t hndlr_lookup_tbl[] = {
    { "volume", hndlr_volume },
    { "sleep", hndlr_sleep },
    { NULL, NULL }
                };

// -----------------  INIT  -------------------------------------------------

void proc_cmd_init(void)
{
    pthread_t tid;

    grammar_init("grammar.syntax", hndlr_lookup_tbl);
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
        INFO("match=%d, args=  '%s'  '%s'  '%s'\n", match, args[0], args[1], args[2]);
        if (match) {
            proc(args);
        } else {
            t2s_play("Don't grok: %s.", cmd);   // xxx rename to transcript
        }

        // done with this cmd
        free(cmd);
        cmd = NULL;
        cancel = false;
    }

    return NULL;
}

// -----------------  HNDLR - VOLUME  ---------------------------------------

static int hndlr_volume(args_t args)
{
    char *action = args[0];
    char *amount = args[1];

    if (strmatch(action, "get", "query", "what", NULL)) {
        t2s_play("The volume is %d%%.", t2s_get_volume());
        return 0;  // xxx how is this ret used
    }

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

    return 0;
}

// -----------------  HNDLR - SLEEP  ----------------------------------------

static int hndlr_sleep(args_t args)
{
    int secs = getnum(args[0], 10);
    
    t2s_play("Sleeping for %d seconds.", secs);

    for (int i = 0; i < secs; i++) {
        sleep(1);
        if (cancel) {
            t2s_play("sleep has been cancelled");
            break;
        }
    }

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
