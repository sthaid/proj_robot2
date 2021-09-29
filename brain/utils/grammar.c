#include <utils.h>

#define MAX_GRAMMAR 1000

typedef struct {
    hndlr_t  proc;
    char   * default_args[10];
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
    char    *default_args[10];
    char     s[10000];
    int      n;
    def_t    def[MAX_DEF];
    int      max_def;
    int      line_num;

    // xxx memory leaks
    // - def[]/name,value should be freed
    // - if called multiple times, grammar should be freed

    // init 
    fp = NULL;
    max_def_save = 0;
    proc = NULL;
    memset(default_args, 0, sizeof(default_args));
    memset(s, 0, sizeof(s));
    n = 0;
    memset(def, 0, sizeof(def));
    max_def = 0;
    line_num = 0;

    max_grammar = 0;

    // open
    fp = fopen(filename, "r");
    if (fp == NULL) {
        ERROR("failed to open %s, %s\n", filename, strerror(errno));
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

        // DEF line
        if (strncmp(s, "DEFINE ", 7) == 0) {
            char *name, *value="", *p;
            name = s+7;
            if (name[0] == '\0') {
                ERROR("line %d: '%s'\n", line_num, s);
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
                ERROR("line %d: '%s'\n", line_num, s);
                goto error;
            }
            max_def_save = max_def;
            default_args[0] = strdup(s+6);
            proc = lookup_hndlr(s+6, hlu);
            if (proc == NULL) {
                ERROR("line %d: '%s'\n", line_num, s);
                goto error;
            }

        // DEFAULT_ARGn line
        } else if (sscanf(s, "DEFAULT_ARG%d ", &n) == 1) {
            if (n < 1 || n > 9 || proc == NULL) {
                ERROR("line %d: '%s'\n", line_num, s);
                goto error;
            }
            default_args[n] = strdup(s+13);

        // END line
        } else if (strncmp(s, "END", 3) == 0) {
            if (proc == NULL) {
                ERROR("line %d: '%s'\n", line_num, s);
                goto error;
            }

            max_def = max_def_save;
            proc = NULL;
            memset(default_args, 0, sizeof(default_args));

        // grammar syntax line
        } else {
            if (proc == NULL) {
                ERROR("line %d: '%s'\n", line_num, s);
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
                ERROR("line %d: '%s'\n", line_num, s);
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

    *proc = NULL;

    strcpy(cmd, cmd_arg);
    sanitize(cmd);
    cmd_len = strlen(cmd);

    for (i = 0; i < max_grammar; i++) {
        grammar_t *g = &grammar[i];

        for (j = 0; j < 10; j++) args[j][0] = '\0';

        if ((match_len = match(g->syntax, cmd, args)) && match_len == cmd_len) {
            *proc = g->proc;
            for (j = 0; j < 10; j++) {
                if (args[j][0] == '\0' && g->default_args[j] != NULL) {
                    strcpy(args[j], g->default_args[j]);
                }
            }
            return true;
        }
    }

    return false;
}

// xxx comments
// xxx optimize
static int match(char *syntax, char *cmd, args_t args)
{
    char token[1000];
    int token_len;

    // loop over the syntax tokens
    int total_match_len = 0;
    while (true) {
        // get the next token
        get_token(syntax, token, &token_len);

        int match_len;

        if (token[0] >= '1' && token[0] <= '9' && token[1] == '=') {
            int n = token[0] - '0';
            memmove(token, token+2, token_len);
            match_len = match(token, cmd, args);
            if (match_len) {
                memcpy(args[n], cmd, match_len);
                args[n][match_len] = '\0';
            }

        } else if (token[0] == '<') {
            token[token_len-1] = '\0';
            memmove(token, token+1, token_len);

            char *txx = token;
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

        } else if (token[0] == '[') {
            token[token_len-1] = '\0';
            memmove(token, token+1, token_len);  // xxx just use a ptr
            match_len = match(token, cmd, args);

        } else if (token[0] == '(') {
            token[token_len-1] = '\0';
            memmove(token, token+1, token_len);
            match_len = match(token, cmd, args);
            if (match_len == 0) {
                return 0;
            }
        } else {
            char *p = cmd, save;
            while (*p != ' ' && *p != '\0') {
                p++;
            }
            save = *p;
            *p = '\0';

            if (strcmp(token, "NUMBER") == 0) {
                double tmp;
                if (sscanf(cmd, "%lf", &tmp) != 1) {
                    *p = save;
                    return 0;
                }
            } else {
                if (strcmp(cmd, token) != 0) {
                    *p = save;
                    return 0;
                }
            }
            match_len = p - cmd;
            *p = save;
        }

        if (match_len) total_match_len += match_len + 1;

        if (syntax[token_len] == '\0') {
            return total_match_len ? total_match_len - 1 : 0;
        }

        // advance cmd
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
        ((p[1] == '=') && (p[2] >= 'a' && p[2] <= 'z')))
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

