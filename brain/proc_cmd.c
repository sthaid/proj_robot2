#include <common.h>

// variables
static char *cmd;
static bool cancel;

// prototypes
static void *cmd_thread(void *cx);

// handlers
static int rotate_hndlr(args_t args);
static int test_hndlr(args_t args);

static hndlr_lookup_t hndlr_lookup_tbl[] = {
    { "rotate", rotate_hndlr },
    { "test",   test_hndlr },
                };

// -----------------  INIT  -------------------------------------------------

void proc_cmd_init(void)
{
    pthread_t tid;

    grammar_init("grammar.syntax", hndlr_lookup_tbl);
    pthread_create(&tid, NULL, cmd_thread, NULL);
}

// -----------------  RUNTIME API  ------------------------------------------

void proc_cmd_execute(char *transcript)
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
        }

        // done with this cmd
        free(cmd);
        cmd = NULL;
        cancel = false;
    }

    return NULL;
}

// -----------------  CMD HANDLERS  -----------------------------------------

static int rotate_hndlr(args_t args)
{
    INFO("rotate: proc=%s amount=%s dir=%s\n", args[0], args[1], args[2]);
    for (int i=0; i < 10; i++) {
        INFO("args[%d] = %s\n", i, args[i]);
    }

    return 0;
}

static int test_hndlr(args_t args)
{
    INFO("test: proc=%s word=%s\n", args[0], args[1]);
    for (int i=0; i < 10; i++) {
        INFO("args[%d] = %s\n", i, args[i]);
    }

    return 0;
}
