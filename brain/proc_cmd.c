#include <common.h>

// variables
static pthread_t proc_cmd_thread_tid;
static char *cmd;
static bool cancel;
static bool prog_terminating;

// prototypes
static void *proc_cmd_thread(void *cx);
static void grammar_find_match(char *cmd, hndlr_t *proc, args_t args);
static int grammar_read_file(void);

// -----------------  INIT & EXIT  ------------------------------------------

void proc_cmd_init(void)
{
    grammar_read_file();
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
    hndlr_t proc;
    args_t args;

    while (true) {
        // wait for cmd
        while (cmd == NULL) {
            if (prog_terminating) return NULL;
            usleep(10000);
        }

        // check if cmd matches known grammar
        grammar_find_match(cmd, &proc, args);

        // if grammar match found then call the handler
        if (proc) {
            proc(args);
        }

        // free the cmd, xxx etc
        free(cmd);
        cmd = NULL;
        cancel = false;
    }
}
#if 0
        // scan the grammar table for a match to cmd  XXX comment
        found = scan_grammar(cmd, proc, args);
        if (found) {
            g->proc(args);
        }
        if (i == max_grammar) {
            error no match
        }

        // if match found then 
        //   call the handler
        // else
        //   error
        // endif

        // xxx set leds
        // - red .5 secs = error,  and audible error tone or text while red
        // - green fo 0.5 secs of a success tone
#endif

#if 0
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
#endif

// -----------------  GRAMMAR  ----------------------------------------------

// xxx comment the syntax here, and the file format

#define MAX_DEF 100
#define MAX_GRAMMAR 1000

typedef struct {
    char *name;
    char *value;
} def_t;

typedef struct {
    hndlr_t  proc;
    char   * default_args[10];
    char   * syntax;
} grammar_t;

def_t def[MAX_DEF];
int max_def;
grammar_t grammar[MAX_GRAMMAR];
int max_grammar;

static bool match(char *syntax, char *s, args_t args);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

static void grammar_find_match(char *cmd, hndlr_t *proc, args_t args)
{
    int i,j;
    grammar_t *g;

    *proc = NULL;

    for (i = 0; i < max_grammar; i++) {
        g = &grammar[i];

        for (j = 0; j < 10; j++) {
            args[j][0] = '\0';
        }

        if (match(g->syntax, cmd, args)) {
            *proc = g->proc;
            for (j = 0; j < 10; j++) {
                if (args[j][0] == '\0' && g->default_args[j] != NULL) {
                    strcpy(args[j], g->default_args[j]);
                }
            }
            return;
        }
    }
}

static bool match(char *syntax, char *s, args_t args)
{
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

static int grammar_read_file(void)
{
    FILE      *fp;
    grammar_t *g;
    char       sx[10000];
    int        max_def_save, n;

    // open
    fp = fopen("proc_cmds.txt", "r");
    if (fp == NULL) {
        ERROR("failed to open proc_cmds.txt, %s\n", strerror(errno));
        return -1;
    }

    // read lines
    while (memset(sx,0,sizeof(sx)), fgets(sx, sizeof(sx), fp) != NULL) {
        char *s = sx;
        int len, n;

        // remove double spaces and spaces at the begining and end,
        // also remove trailing newline
        len = strlen(s);
        while (len > 0 && (s[len-1] == '\n' || s[len-1] == ' ')) {
            s[len-1] = '\0';
            len--;
        }
        n = strspn(s, " ");
        memmove(s, s+n, len+1);

        // if line is blank, or a comment then continue
        if (s[0] == '\0' || s[0] == '\n') {
            continue;
        }

        // DEF line
        if (strncmp(s, "DEFINE ", 7) == 0) {
            char *name = s+7;
            char *value = name + strlen(name) + 1;
            if (name[0] == '\0' || value[0] == '\0') {
                goto error;
            }
            def[max_def].name = strdup(name);
            def[max_def].value = strdup(value);
            max_def++;

        // HNDLR line
        } else if (strncmp(s, "HNDLR ", 6) == 0) {
            if (g != NULL) {
                goto error;
            }
            max_def_save = max_def;
            g = &grammar[max_grammar];
            g->default_args[0] = strdup(s+6);
            g->proc = proc_cmd_lookup_hndlr(s+6);

        // DEFAULT_ARGn line
        } else if (sscanf(s, "DEFAULT_ARG%d ", &n) == 1) {
            if (n < 1 || n > 9) {
                goto error;
            }
            if (g == NULL) {
                goto error;
            }
            g->default_args[n] = strdup(s+13);

        // END line
        } else if (strncmp(s, "END ", 4) == 0) {
            if (g == NULL) {
                goto error;
            }
            max_def = max_def_save;
            max_grammar++;
            g = NULL;

#if 0
        // grammar line
        } else {
            if (g == NULL;) {
                ERROR
            }
            s1 = substitute_defines(s);
            g->line[g->max_line++] = s1;
#endif
        }
    }

    // close
    fclose(fp);

    // debug print the grammar table

    // success
    return 0;

    // error
error:
    if (fp) fclose(fp);
    return -1;
}

// -----------------------------------------------------
// -----------------------------------------------------
// -----------------------------------------------------

#if 0
predefined   NUMBER  WORD

DEFINE COLOR  <red orange yellow green blue purple (shocking pink)>

HNDLR rotate
DEFAULT_ARG1  all the way around
DEFAULT_ARG2  clockwise
DEFINE AMOUNT    <(NUMBER degrees)  (around)  (half way around)  (all [of] the way around)>
DEFINE DIRECTION <clockwise  counterclockwise>
[robot] <rotate turn> 1=[AMOUNT] 2=[DIRECTION]
END

HNDLR values_set
DEFINE AGE  NUMBER_1_TO_99
my 1=favorite-color is 2=COLOR
my 1=age            is 2=AGE
my 1=name           is 2=WORD
END

bool match(pattern, is-single-tok, str)
{
    if pattern is a single token
        word            adv-str
        anyword            adv-str
        number            adv-str
        X=xxxx
        (aaa bcd)
        [aaa bbb ccc <ddd eee>]
        <clockwise counterclockwise>
    endif

    loop over all tokens in pattern
        if match(token, is-single str) == false return false
    endloop
}
#endif

