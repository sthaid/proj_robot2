// XXX 
// - don't use a longer cmd str
// - cleanup

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

    args_t args;
    hndlr_t proc;
    printf("---------- TEST 1 -----------\n");
    //grammar_find_match("hello ch3 ch4 again XXX", &proc, args);

    char cmd[100];
    memset(cmd, 'x', sizeof(cmd));
    strcpy(cmd, "simple test optional2 endofline");
    grammar_find_match(cmd, &proc, args);


    if (proc) proc(args);

#if 0
    printf("---------- TEST 2 -----------\n");
    grammar_find_match("hello choice2 again XXX", &proc, args);
    printf("---------- TEST 3 -----------\n");
    grammar_find_match("hello badbad again XXX", &proc, args);
    printf("---------- TEST 4 -----------\n");
    grammar_find_match("hello again XXX", &proc, args);
#endif

    printf("*** EXIT\n");
    exit(1);

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

void proc_cmd_cancel(void)
{
    cancel = true;
}

// -----------------  PROC_CMD_THREAD  --------------------------------------

static void *proc_cmd_thread(void *cx)
{
    hndlr_t proc;
    args_t args;

// XXX
    return NULL;

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
static int       max_def;  // xxx can be in the read proc
static grammar_t grammar[MAX_GRAMMAR];
static int       max_grammar;

// ------------

static int match(char *syntax, char *cmd, args_t args);
static void get_token(char *syntax, char *token, int *token_len);
static void get_first_word(char *cmd, char *first_word);
static char *args_str(args_t args);

static void grammar_find_match(char *cmd, hndlr_t *proc, args_t args)
{
    int i,j;
    grammar_t *g;

    *proc = NULL;

    for (i = 0; i < max_grammar; i++) {
        g = &grammar[i];

        printf("XXXXX[%d] = '%s'\n", i, g->syntax);

// xxx move this ?
        for (j = 0; j < 10; j++) {
            args[j][0] = '\0';
        }

        int match_len;
        int len_cmd = strlen(cmd);
        if ((match_len = match(g->syntax, cmd, args)) && match_len == len_cmd) {
            *proc = g->proc;
            for (j = 0; j < 10; j++) {
                if (args[j][0] == '\0' && g->default_args[j] != NULL) {
                    strcpy(args[j], g->default_args[j]);
                }
            }
            printf("ARGS = %s\n", args_str(args));
            printf("--------------------------------------\n");

            return;
        }
    }
}

static char *args_str(args_t args)
{
    static char s[1000], *p = s;
    for (int i = 0; i < 10; i++) {
        if (args[i][0] != '\0') {
            p += sprintf(p, "%d='%s' ", i, args[i]);
        }
    }
    return s;
}

// xxx make a do_match routine to run this 
//         and confirm cmd doesn't have leading or trainling spaces

// xxx caller must ensure cmd has no trailing spaces
static int match(char *syntax, char *cmd, args_t args)
{
    char token[1000];
    int token_len;
    char first_word[100];

    // loop over the syntax tokens

    int total_match_len = 0;
    while (true) {
        // get the next token
        get_token(syntax, token, &token_len);
        printf("got token '%s' len=%d\n", token, token_len);

        int match_len;

        if (token[0] >= '1' && token[0] <= '9' && token[1] == '=') {
            int n = token[0] - '0';
            memmove(token, token+2, token_len);
            printf("EQUALS TOKEN = '%s'\n", token);
            match_len = match(token, cmd, args);
            printf("EQUALS match_len = %d\n", match_len);
            if (match_len) {
                memcpy(args[n], cmd, match_len);
                args[n][match_len] = '\0';
            }

        } else if (token[0] == '<') {
            token[token_len-1] = '\0';
            memmove(token, token+1, token_len);
            printf("OR TOKEN = '%s'\n", token);

            char *txx = token;
            while (true) {
                int tl;
                char t[1000];
                get_token(txx, t, &tl);
                printf("OR TOKEN - t='%s' cmd='%s'\n", t, cmd);
                match_len = match(t, cmd, args);
                printf("OR TOKEN - match_len = %d\n", match_len);
                if (match_len) {
                    break;
                }
                if (txx[tl] == '\0') {
                    printf("OR TOKEN - DONE, return 0\n");
                    return 0;
                }
                txx += tl + 1;
            }

        } else if (token[0] == '[') {
            token[token_len-1] = '\0';
            memmove(token, token+1, token_len);  // xxx just use a ptr
            printf("BRACKETS TOKEN = '%s'\n", token);
            match_len = match(token, cmd, args);
            printf("BRACKETS match_len = %d\n", match_len);

        } else if (token[0] == '(') {
            token[token_len-1] = '\0';
            memmove(token, token+1, token_len);
            printf("PAREN TOKEN = '%s'\n", token);
            match_len = match(token, cmd, args);
            if (match_len == 0) {
                printf("PAREN MISCOMPARE RET false\n");
                return 0;
            }
        } else {
            get_first_word(cmd, first_word);
            printf("COMPARE FIRST_WORD = '%s' TO TOKEN = '%s'\n", first_word, token);
            if (strcmp(first_word, token) != 0) {
                printf("MISCOMPARE RET false\n");
                return 0;
            }
            match_len = strlen(first_word);
        }

        if (match_len) total_match_len += match_len + 1;
        printf("tml = %d\n", total_match_len);

        if (syntax[token_len] == '\0') {
            printf("MATCH OKAY, ret matchlen = %d\n", total_match_len - 1);
            return total_match_len ? total_match_len - 1 : 0;
        }

        // advance cmd
        if (match_len) {
            cmd += match_len;
            if (*cmd == ' ') cmd++;
            printf("XXX UPDATED CMD TO '%s'\n", cmd);
        }

        // advance syntax to point to the next token
        syntax += token_len + 1;
    }
}

// xxx maybe don't need this
static void get_first_word(char *cmd, char *first_word)
{
    printf("GFW called for  '%s'\n", cmd);
    int len = strcspn(cmd, " ");
    if (cmd[0] == ' ') {
        printf("XXXXXXXXXXXXXXXXXXXXX len=%d BUG BUG\n", len);
    }
    memcpy(first_word, cmd, len);
    first_word[len] = '\0';
}

static void get_token(char *syntax, char *token, int *token_len)
{
    int cnt;
    char *p;

    // skip leading spaces
    // xxx is this needed
    while (*syntax == ' ') {
        syntax++;
    }

    p = syntax;

#if 1
    if (*p == '\0') {
        *token_len = 0;
        token[0] = '\0';
        FATAL("BUG\n");
        return;
    }        
#endif

    cnt = 0;
    while (true) {
        if (*p == '(' || *p == '[' || *p == '<') {
            cnt++;
        } else if (*p == ')' || *p == ']' || *p == '>') {
            cnt--;
        }

        if (cnt < 0 || (*p == '\0' && cnt)) {
            //printf("cnt %d  offset=%d  char=%c\n", cnt, p-syntax, *p);
            FATAL("invalid syntax '%s'\n", syntax);
        }

        if ((cnt == 0) && (*p == ' ' || *p == '\0')) {
            *token_len = p - syntax;
            memcpy(token, syntax, *token_len);
            token[*token_len] = '\0';
            return;
        }

        p++;
    }
}

// ---------

static void sanitize(char *s);
static void substitute(char *s, char *current, char *replace);
static int check_syntax(char *s);

static int grammar_read_file(void)
{
    FILE    *fp;
    int      max_def_save;
    hndlr_t  proc;
    char    *default_args[10];
    char     s[10000];
    int      n;

    // xxx init  (also free)
    max_def = 0;
    max_grammar = 0;

    // init local vars
    fp = NULL;
    max_def_save = 0;
    proc = NULL;
    memset(default_args, 0, sizeof(default_args));
    memset(s, 0, sizeof(s));
    n = 0;

    // open
    fp = fopen("proc_cmd.txt", "r");
    if (fp == NULL) {
        ERROR("failed to open proc_cmd.txt, %s\n", strerror(errno));
        return -1;
    }

    // read lines
    while (memset(s,0,sizeof(s)), fgets(s, sizeof(s), fp) != NULL) {
        //printf("LINE '%s'\n", s);
            
        // sanitize the line
        // - removes newline at end
        // - removes spaces at the begining and end
        // - replaces double spaces with single spaces
        sanitize(s);
        // xxx this removes double spaces that you might want in args
        
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
            def[max_def].name = strdup(name);
            def[max_def].value = strdup(value);
            max_def++;

        // HNDLR line
        } else if (strncmp(s, "HNDLR ", 6) == 0) {
            if (proc) {
                ERROR("XXX %d\n", __LINE__);
                goto error;
            }
            max_def_save = max_def;
            default_args[0] = strdup(s+6);
            proc = proc_cmd_lookup_hndlr(s+6);
            printf("HNDLR: arg[0]='%s'  proc=%p\n", default_args[0], proc);
            if (proc == NULL) {
                ERROR("XXX %d\n", __LINE__);
                goto error;
            }

        // DEFAULT_ARGn line
        } else if (sscanf(s, "DEFAULT_ARG%d ", &n) == 1) {
            if (n < 1 || n > 9) {
                ERROR("XXX %d\n", __LINE__);
                goto error;
            }
            if (proc == NULL) {
                ERROR("XXX %d\n", __LINE__);
                goto error;
            }
            default_args[n] = strdup(s+13);
            printf("DEFAULT_ARG %d = '%s'\n", n, default_args[n]);

        // END line
        } else if (strncmp(s, "END", 3) == 0) {
            if (proc == NULL) {
                ERROR("XXX %d\n", __LINE__);
                goto error;
            }
            printf("END: \n");

            max_def = max_def_save;
            proc = NULL;
            memset(default_args, 0, sizeof(default_args));

        // XXX temp
        } else if (strncmp(s, "EOF", 3) == 0) {
            printf("XXX EOF\n");
            break;

        // grammar syntax line
        } else {
            if (proc == NULL) {
                ERROR("XXX %d\n", __LINE__);
                goto error;
            }

            for (int i = 0; i < max_def; i++) {
                substitute(s, def[i].name, def[i].value);
            }

            sanitize(s);

            substitute(s, "( ", "(");
            substitute(s, " )", ")");
            substitute(s, "[ ", "[");
            substitute(s, " ]", "]");
            substitute(s, "< ", "<");
            substitute(s, " >", ">");

#if 0
            for (char *p = "([<)]>"; *p; p++) {
                static char replace[4] = "   ";
                static char current[2] = " ";
                current[0] = *p;
                replace[1] = *p;
                substitute(s, current, replace);
            }
#endif


            printf("SYNTAX: %s\n", s);

            if (check_syntax(s) == -1) {
                ERROR("XXX %d\n", __LINE__);
                goto error;
            }

            grammar_t *g = &grammar[max_grammar++];
            g->proc = proc;
            memcpy(g->default_args, default_args, sizeof(g->default_args));
            g->syntax = strdup(s);
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

static void sanitize(char *s)
{
    int len = strlen(s);
    if (len > 0 && s[len-1] == '\n') {
        s[len-1] = '\0';
        len--;
    }

    while (len > 0 && s[len-1] == ' ') {
        s[len-1] = '\0';
        len--;
    }

    int n = strspn(s, " ");
    memmove(s, s+n, len+1);

    for (char *p = s; (p = strstr(p, "  ")); ) {
        memmove(p, p+1, strlen(p));
    }
}

static void substitute(char *s, char *current, char *replace)
{
    int len_current = strlen(current);
    int len_replace = strlen(replace);
    char *p;

    while ((p = strstr(s, current))) {
        memmove(p, p+len_current, strlen(p)+1);
        memmove(p+len_replace, p, strlen(p)+1);
        memcpy(p, replace, len_replace);
        s = p + len_replace;
    }
}

static int check_syntax(char *s)
{
    int cnt1=0, cnt2=0, cnt3=0;

    for (; *s; s++) {
        switch (*s) {
        case '(': cnt1++; break;
        case ')': cnt1--; break;
        case '[': cnt2++; break;
        case ']': cnt2--; break;
        case '<': cnt3++; break;
        case '>': cnt3--; break;
        }
        if (cnt1 < 0 || cnt2 < 0 || cnt3 < 0) {
            return -1;
        }
    }

    if (cnt1 != 0 || cnt2 != 0 || cnt3 != 0) {
        return -1;
    }

    return 0;
}

#if 0
        // remove spaces at the begining and end, and newline char, and 
        // replace multiple spaces with single space

            // sanity check syntax
            check_syntax();
            if (cnt1 || cnt2 || cnt3) {
                ERROR("XXX %d - %d %d %d\n", __LINE__, cnt1, cnt2, cnt3);
                goto error;
            }

xxx substitute routine
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
}
#endif

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

