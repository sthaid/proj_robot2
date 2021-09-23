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

    printf("*** EXIT\n");
    exit(1);
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

void proc_cmd_cancel(void)
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

static           def_t def[MAX_DEF];
static int       max_def;
static grammar_t grammar[MAX_GRAMMAR];
static int       max_grammar;

static bool match(char *syntax, char *s, args_t args);
static void substitute_defs(char *s);
static void add_def(char *name, char *value);

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
    return false;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

static int grammar_read_file(void)
{
    FILE      *fp;
    grammar_t *g;
    char       sx[10000];
    int        max_def_save;

    // xxx should free first

    g = NULL;
    fp = NULL;
    max_def_save = 0;

    max_def = 0;
    max_grammar = 0;

    add_def(" )", ")");
    add_def(" >", ">");
    add_def(" ]", "]");
    add_def("( ", "(");
    add_def("< ", "<");
    add_def("[ ", "[");

    // open
    fp = fopen("proc_cmd.txt", "r");
    if (fp == NULL) {
        ERROR("failed to open proc_cmd.txt, %s\n", strerror(errno));
        return -1;
    }

    // read lines
    while (memset(sx,0,sizeof(sx)), fgets(sx, sizeof(sx), fp) != NULL) {
        char *s = sx, *p;
        int len, n;

        // remove spaces at the begining and end, and newline char, and 
        // replace multiple spaces with single space
        len = strlen(s);
        while (len > 0 && (s[len-1] == '\n' || s[len-1] == ' ')) {
            s[len-1] = '\0';
            len--;
        }

        n = strspn(s, " ");
        memmove(s, s+n, len+1);

        while ((p = strstr(s, "  "))) {
            memmove(p, p+1, strlen(p)+1);
        }

        //printf("LINE '%s'\n", s);
            
        // if line is blank, or a comment then continue
        if (s[0] == '\0' || s[0] == '#') {
            continue;
        }

        // DEF line
        if (strncmp(s, "DEFINE ", 7) == 0) {
            char *name, *value="", *p;
            name = s+7;
            if (name[0] == '\0') {
                ERROR("XXX %d\n", __LINE__);
                goto error;
            }
            if ((p = strchr(name, ' '))) {
                *p = '\0';
                value = p+1;
            }
            printf("DEFINE '%s' '%s'\n", name, value);
            add_def(name, value);

        // HNDLR line
        } else if (strncmp(s, "HNDLR ", 6) == 0) {
            if (g != NULL) {
                ERROR("XXX %d\n", __LINE__);
                goto error;
            }
            max_def_save = max_def;
            g = &grammar[max_grammar];
            g->default_args[0] = strdup(s+6);
            g->proc = proc_cmd_lookup_hndlr(s+6);
            printf("HNDLR: arg[0]='%s'  proc=%p\n", g->default_args[0], g->proc);
            if (g->proc == NULL) {
                ERROR("XXX %d\n", __LINE__);
                goto error;
            }

        // DEFAULT_ARGn line
        } else if (sscanf(s, "DEFAULT_ARG%d ", &n) == 1) {
            if (n < 1 || n > 9) {
                ERROR("XXX %d\n", __LINE__);
                goto error;
            }
            if (g == NULL) {
                ERROR("XXX %d\n", __LINE__);
                goto error;
            }
            g->default_args[n] = strdup(s+13);
            printf("DEFAULT_ARG %d = '%s'\n", n, g->default_args[n]);

        // END line
        } else if (strncmp(s, "END", 3) == 0) {
            if (g == NULL) {
                ERROR("XXX %d\n", __LINE__);
                goto error;
            }
            max_def = max_def_save;
            max_grammar++;
            g = NULL;
            printf("END: \n");

        // XXX temp
        } else if (strncmp(s, "EOF", 3) == 0) {
            printf("XXX EOF\n");
            break;

        // grammar line
        } else {
            if (g == NULL) {
                ERROR("XXX %d\n", __LINE__);
                goto error;
            }
            substitute_defs(s);
            g->syntax = strdup(s);
            printf("SYNTAX: %s\n", g->syntax);

            // xxx check 100 chars after for not 0

            // sanity check syntax
            int i, len=strlen(s), cnt1=0, cnt2=0, cnt3=0;
            for (i = 0; i < len; i++) {
                switch (s[i]) {
                case '(': cnt1++; break;
                case ')': cnt1--; break;
                case '[': cnt2++; break;
                case ']': cnt2--; break;
                case '<': cnt3++; break;
                case '>': cnt3--; break;
                }
            }
            if (cnt1 || cnt2 || cnt3) {
                ERROR("XXX %d - %d %d %d\n", __LINE__, cnt1, cnt2, cnt3);
                goto error;
            }
        }
    }

    // close
    fclose(fp);

    // debug print the grammar table
    INFO("max_grammar = %d\n", max_grammar);
    for (int i = 0; i < max_grammar; i++) {
        char str[1000]={0}, *p=str;
        grammar_t *g = &grammar[i];
        for (int j = 0; j < 10; j++) {
            if (g->default_args[j]) {
                p += sprintf(p, "%d='%s' ", j, g->default_args[j]);
            }
        }
        INFO("grammar[%d]: %s\n", i, str);
        INFO("  %s\n", g->syntax);
    }

    // success
    return 0;

    // error
error:
    if (fp) fclose(fp);
    return -1;
}

static void substitute_defs(char *s)
{
    int i;
    char *p, *name, *value;

    for (i = max_def-1; i >= 0; i--) {
        name = def[i].name;
        value = def[i].value;
        while ((p = strstr(s, name))) {
            memmove(p, p+strlen(name), strlen(p)+1);
            memmove(p+strlen(value), p, strlen(p)+1);
            memcpy(p, value, strlen(value));
        }
    }
}

static void add_def(char *name, char *value)
{
    def[max_def].name = strdup(name);
    def[max_def].value = strdup(value);
    max_def++;
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


hello[planet world]
hello [planet world]

#endif

