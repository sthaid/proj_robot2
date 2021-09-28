#include <common.h>

// variables
static pthread_t cmd_thread_tid;
static char *cmd;
static bool cancel;
static bool prog_terminating;

// prototypes
static void *cmd_thread(void *cx);
static int rotate_hndlr(args_t args);

// handlers lookup table
static hndlr_lookup_t hndlr_lookup_tbl[] = {
    { "rotate", rotate_hndlr },
                };

// -----------------  INIT & EXIT  ------------------------------------------

void proc_cmd_init(void)
{
    grammar_init("grammar.syntax", hndlr_lookup_tbl);
    pthread_create(&cmd_thread_tid, NULL, cmd_thread, NULL);

}

void proc_cmd_exit(void) // xxx is this called
{
    prog_terminating = true;
    pthread_join(cmd_thread_tid, NULL);
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
            if (prog_terminating) return NULL;
            usleep(10000);
        }

        // check if cmd matches known grammar
        grammar_match(cmd, &proc, args);

        // if grammar match found then call the handler
        // xxx else ?
        if (proc) {
            proc(args);
        }

        // free the cmd, xxx etc
        free(cmd);
        cmd = NULL;
        cancel = false;
    }
}

// -----------------  CMD HANDLERS  -----------------------------------------

static int rotate_hndlr(args_t args)
{
    INFO("rotate: degrees=%s  dir=%s\n", args[0], args[1]);

    for (int i=0; i < 10; i++) {
        INFO("args[%d] = %s\n", i, args[i]);
    }

    return 0;
}
