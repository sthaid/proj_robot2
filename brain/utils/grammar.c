#include <utils.h>

#define MAX_GRAMMAR 1000

typedef struct {
    hndlr_t  proc;
    char   * syntax;
} grammar_t;

static grammar_t grammar[MAX_GRAMMAR];
static int       max_grammar;

// -----------------  GRAMMAR INIT  ------------------------------------------

static void sanitize(char *s);
static void substitute(char *s, char *current, char *replace);
static int check_syntax(char *s);
static hndlr_t lookup_hndlr(char *name, hndlr_lookup_t *hlu);

int grammar_init(char *filename, hndlr_lookup_t *hlu)
{
    #define MAX_DEF 100

    typedef struct {
        char *name;
        char *value;
    } def_t;

    FILE    *fp;
    int      max_def_save;
    hndlr_t  proc;
    char     s[10000];
    def_t    def[MAX_DEF];
    int      max_def;
    int      line_num;

    // some memory leaks in here, but don't worry they are small
    // and this routine is only called once at progarm initialization

    // init local vars
    fp = NULL;
    max_def_save = 0;
    proc = NULL;
    memset(s, 0, sizeof(s));
    memset(def, 0, sizeof(def));
    max_def = 0;
    line_num = 0;

    // init global vars
    max_grammar = 0;

    // open
    fp = fopen(filename, "r");
    if (fp == NULL) {
        FATAL("failed to open %s, %s\n", filename, strerror(errno));
        return -1;
    }

    // read lines
    while (memset(s,0,sizeof(s)), fgets(s, sizeof(s), fp) != NULL) {
        line_num++;

        // sanitize the line
        // - removes newline at end
        // - removes spaces at the begining and end
        // - replaces multiple spaces with single spaces
        sanitize(s);

        // if line is blank, or a comment then continue
        if (s[0] == '\0' || s[0] == '#') {
            continue;
        }

        // DEFINE line
        if (strncmp(s, "DEFINE ", 7) == 0) {
            char *name, *value="", *p;
            name = s+7;
            if (name[0] == '\0') {
                FATAL("line %d: '%s'\n", line_num, s);
                goto error;
            }
            if ((p = strchr(name, ' '))) {
                *p = '\0';
                value = p+1;
            }
            def[max_def].name = strdup(name);
            def[max_def].value = strdup(value);
            max_def++;

        // HNDLR line
        } else if (strncmp(s, "HNDLR ", 6) == 0) {
            if (proc) {
                FATAL("line %d: '%s'\n", line_num, s);
                goto error;
            }
            max_def_save = max_def;
            proc = lookup_hndlr(s+6, hlu);
            if (proc == NULL) {
                FATAL("line %d: '%s'\n", line_num, s);
                goto error;
            }

        // END line
        } else if (strcmp(s, "END") == 0) {
            if (proc == NULL) {
                FATAL("line %d: '%s'\n", line_num, s);
                goto error;
            }

            max_def = max_def_save;
            proc = NULL;

        // EXIT line
        } else if (strcmp(s, "EXIT") == 0) {
            break;

        // grammar syntax line
        } else {
            if (proc == NULL) {
                FATAL("line %d: '%s'\n", line_num, s);
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

            if (check_syntax(s) == -1) {
                FATAL("line %d: '%s'\n", line_num, s);
                goto error;
            }

            grammar_t *g = &grammar[max_grammar++];
            g->proc = proc;
            g->syntax = strdup(s);
        }
    }

    // close
    fclose(fp);

#if 0
    // debug print the grammar table
    INFO("max_grammar = %d\n", max_grammar);
    for (int i = 0; i < max_grammar; i++) {
        INFO("'%s'\n", grammar[i].syntax);
    }
#endif

    // success
    return 0;

    // error
error:
    if (fp) fclose(fp);
    return -1;
}

static void sanitize(char *s)
{
    // remove newline at eol
    int len = strlen(s);
    if (len > 0 && s[len-1] == '\n') {
        s[len-1] = '\0';
        len--;
    }

    // remove spaces at eol
    while (len > 0 && s[len-1] == ' ') {
        s[len-1] = '\0';
        len--;
    }

    // remove spaces at begining of line
    int n = strspn(s, " ");
    memmove(s, s+n, len+1);

    // replace multiple spaces with single space
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

static hndlr_t lookup_hndlr(char *name, hndlr_lookup_t *hlu)
{
    while (hlu->proc) {
        if (strcmp(name, hlu->name) == 0) {
            return hlu->proc;
        }
        hlu++;
    }
    return NULL;
}

// -----------------  GRAMMAR MATCH  -----------------------------------------

static int match(char *syntax, char *cmd, args_t args);
static void get_token(char *syntax, char *token, int *token_len);
static char *args_str(args_t args) __attribute__((unused));

bool grammar_match(char *cmd_arg, hndlr_t *proc, args_t args)
{
    int i, j, match_len,cmd_len;
    char cmd[1000];

    static struct {
        char *current;
        char *replace;
    } subst_tbl[] = {
        { "zero",  "0" },
        { "one",   "1" },
        { "two",   "2" },
        { "three", "3" },
        { "four",  "4" },
        { "five",  "5" },
        { "six",   "6" },
        { "seven", "7" },
        { "eight", "8" },
        { "nine",  "9" },
            };


    // preset return proc to NULL
    *proc = NULL;

    // make a copy of cmd because cmd is sanitized and converted to lower case hee
    strcpy(cmd, cmd_arg);
    sanitize(cmd);
    for (i = 0; cmd[i]; i++) cmd[i] = tolower(cmd[i]);

    // make substitutions
    for (i = 0; i < sizeof(subst_tbl)/sizeof(subst_tbl[0]); i++) {
        substitute(cmd, subst_tbl[i].current, subst_tbl[i].replace);
    }
    cmd_len = strlen(cmd);

    // loop over grammar table to find a match;
    // if match found return the handler proc to caller
    for (i = 0; i < max_grammar; i++) {
        grammar_t *g = &grammar[i];

        for (j = 0; j < 10; j++) args[j][0] = '\0';
        if ((match_len = match(g->syntax, cmd, args)) && match_len == cmd_len) {
            *proc = g->proc;
            return true;
        }
    }

    // clear args and return no-match
    for (j = 0; j < 10; j++) args[j][0] = '\0';
    return false;
}

static int match(char *syntax, char *cmd, args_t args)
{
    char token[1000];
    int token_len, match_len;
    int total_match_len, is_arg;

    // loop over the syntax tokens
    total_match_len = 0;
    while (true) {
        // get the next token
        get_token(syntax, token, &token_len);

        // if this token is an arg (begins with N:) then set is_arg to
        // remember the arg number, and remove the N: from the beginning
        // of the token
        is_arg = -1;
        if (token[0] >= '0' && token[0] <= '9' && token[1] == ':') {
            is_arg = token[0] - '0';
            memmove(token, token+2, token_len+1);
            token_len -= 2;
            syntax += 2;
        }

        // match on the first token in a list: <token token token ...>  
        if (token[0] == '<') {
            token[token_len-1] = '\0';
            char *txx = token+1;
            while (true) {
                int tl;
                char t[1000];
                get_token(txx, t, &tl);
                match_len = match(t, cmd, args);
                if (match_len) {
                    break;
                }
                if (txx[tl] == '\0') {
                    return 0;
                }
                txx += tl + 1;
            }

        // match is optional: [token]
        } else if (token[0] == '[') {
            token[token_len-1] = '\0';
            match_len = match(token+1, cmd, args);

        // match all: (token token ...)
        } else if (token[0] == '(') {
            token[token_len-1] = '\0';
            match_len = match(token+1, cmd, args);
            if (match_len == 0) {
                return 0;
            }

        // match a word or number or percent
        } else {
            // put a temporary '\0' at the end of the first word of cmd
            char *p = cmd, save;
            while (*p != ' ' && *p != '\0') {
                p++;
            }
            save = *p;
            *p = '\0';

            // check for failed match, and if so then return match_len = 0
            if (strcmp(token, "NUMBER") == 0) {
                double tmp;
                if (sscanf(cmd, "%lf", &tmp) != 1) {
                    *p = save;
                    return 0;
                }
            } else if (strcmp(token, "PERCENT") == 0) {
                double tmp;
                if (sscanf(cmd, "%lf%%", &tmp) != 1) {
                    *p = save;
                    return 0;
                }
            } else if (strcmp(token, "WORD") == 0) {
                // match any word
            } else {
                if (strcmp(cmd, token) != 0) {
                    *p = save;
                    return 0;
                }
            }

            // match okay, match_len is the length of the first word of cmd
            match_len = p - cmd;
            *p = save;
        }

        // if the token that was just processed is an arg then
        // copy the matching string to the return arg buffer
        if (is_arg != -1) {
            memcpy(args[is_arg], cmd, match_len);
            args[is_arg][match_len] = '\0';
        }

        // keep track of the total_macth_len
        if (match_len) total_match_len += match_len + 1;

        // if there are no more tokens then return the total_match_len
        if (syntax[token_len] == '\0') {
            return total_match_len ? total_match_len - 1 : 0;
        }

        // advance cmd to the next word
        if (match_len) {
            cmd += match_len;
            if (*cmd == ' ') cmd++;
        }

        // advance syntax to point to the next token
        syntax += token_len + 1;
    }
}

static void get_token(char *syntax, char *token, int *token_len)
{
    int cnt;
    char *p = syntax;

    // syntax is expected to not start with a space or by an empty string
    if (*p == ' ' || *p == '\0') {
        FATAL("invalid syntax '%s' passed to get_token\n", p);
    }

    // optimized for tokens that do not contain (), [], or <>
    if ((p[0] >= 'a' && p[0] <= 'z') ||
        ((p[1] == ':') && (p[2] >= 'a' && p[2] <= 'z')))
    {
        while (*p != ' ' && *p != '\0') {
            *token++ = *p++;
        }
        *token = '\0';
        *token_len = p - syntax;
        return;
    }

    // tokens that do contain (), [], or <>
    cnt = 0;
    while (true) {
        if (*p == '(' || *p == '[' || *p == '<') {
            cnt++;
        } else if (*p == ')' || *p == ']' || *p == '>') {
            cnt--;
        }

        if (cnt < 0 || (*p == '\0' && cnt)) {
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

