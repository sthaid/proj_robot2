#include <common.h>

// variables
static pthread_t proc_cmd_thread_tid;
static char *cmd;
static bool cancel;
static bool prog_terminating;

// prototypes
static void *proc_cmd_thread(void *cx);

// -----------------  INIT & EXIT  ------------------------------------------

void proc_cmd_init(void)
{
    pthread_create(&proc_cmd_thread_tid, NULL, proc_cmd_thread, NULL);
}

void proc_cmd_exit(void)
{
    prog_terminating = true;
    pthread_join(proc_cmd_thread_tid, NULL);
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

bool proc_cmd_cancel(void)
{
    cancel = true;
}

// -----------------  PROC_CMD_THREAD  --------------------------------------

static void *proc_cmd_thread(void *cx)
{
    while (true) {
        // wait for cmd
        while (cmd == NULL) {
            if (prog_terminating) return NULL;
            usleep(10000);
        }

        // process the cmd
        INFO("cmd = '%s'\n", cmd);
        if (strcmp(cmd, "what time is it") == 0) {
            t2s_play_text("the time is 11:30");
        } else if (strcmp(cmd, "sleep") == 0) {
            t2s_play_text("sleeping for 10 seconds");
            for (int i = 0; i < 10; i++) {
                sleep(1);
                if (cancel) {
                    t2s_play_text("cancelling");
                    break;
                }
            }
        } else if (strcmp(cmd, "exit") == 0) {
            prog_terminating = true;
        } else {
            t2s_play_text("sorry, I can't do that");
        }

        // free the cmd
        free(cmd);
        cmd = NULL;

        cancel = false;
    }
}

