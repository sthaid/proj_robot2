// gcc -Wall -g -I. grammar_test.c grammar.c misc.c -lm -o gt

#include <utils.h>

static int hndlr_test(args_t args);
static int hndlr_rotate(args_t args);

static hndlr_lookup_t hlu[] = {
    { "test",   hndlr_test },
    { "rotate", hndlr_rotate },
    { NULL, NULL } };

struct test_s {
    char *cmd;
    bool  ok;
    char *arg0;
    char *arg1;
    char *arg2;
} test[] = {
    // match okay
    { "rotate 45 degrees",              true, "rotate", "45 degrees", "cw" },
    { "rotate 45 degrees ccw",          true, "rotate", "45 degrees", "ccw" },
    { "turn",                           true, "rotate", "360", "cw" },
    { "turn cw half way around",        true, "rotate", "half way around", "cw" },
    { "turn ccw all the way around",    true, "rotate", "all the way around", "ccw" },
    { "turn ccw all of the way around", true, "rotate", "all of the way around", "ccw" },
    { "test1 next opt2 eol",            true, "test",   "opt2", "eol" },
    { "test1 next opt2",                true, "test",   "opt2", "DEFARG2" },
    { "test1",                          true, "test",   "DEFARG1", "DEFARG2" },
    { "test1 eol",                      true, "test",   "DEFARG1", "eol" },
    { "test1 next",                     true, "test",   "DEFARG1", "DEFARG2" },
    { "test1 opt 3 and 4",              true, "test",   "opt 3 and 4", "DEFARG2" },
    { "test1 opt1",                     true, "test",   "opt1", "DEFARG2" },
    { "test1 next opt1 eol",            true, "test",   "opt1", "eol" },
    { "test2 next",                     true, "test",   "DEFARG1", "DEFARG2" },
    // match not okay
    { "hello world",                    false, "", "", "" },
    { "test1 hello opt1 eol",           false, "", "", "" },
    { "test1 optx eol",                 false, "", "", "" },
    { "test1 opt1 eol toomuch",         false, "", "", "" },
    { "test2",                          false, "", "", "" },
    { "",                               false, "", "", "" },
            };

#define MAX_TEST (sizeof(test)/sizeof(test[0]))

// -----------------  MAIN  -------------------------------------

int main(int argc, char **argv)
{
    int rc;
    bool ok;
    args_t args;
    hndlr_t hndlr;
    int i;

    fp_log = stdout;  //xxx make a routine

    rc = grammar_init("grammar_test.syntax", hlu);
    if (rc < 0) {
        FATAL("grammar_init failed\n");
    }

    for (i = 0; i < MAX_TEST; i++) {
        struct test_s *t = &test[i];
        ok = grammar_match(t->cmd, &hndlr, args);

        if (ok == t->ok && 
            strcmp(args[0], t->arg0) == 0 &&
            strcmp(args[1], t->arg1) == 0 &&
            strcmp(args[2], t->arg2) == 0) 
        {
            printf("TEST %d: OKAY\n", i);
        } else {
            printf("TEST %d: FAILED\n", i);
            printf("cmd   = '%s'\n", t->cmd);
            printf("match = %s\n", (ok ? "true" : "false"));
            printf("arg0  = expected:'%s' actual:'%s'\n", t->arg0, args[0]);
            printf("arg1  = expected:'%s' actual:'%s'\n", t->arg1, args[1]);
            printf("arg2  = expected:'%s' actual:'%s'\n", t->arg2, args[2]);
            exit(1);
        }
    }

    return 0;
}

// -----------------  HANDLERS  ---------------------------------

static int hndlr_test(args_t args)
{
    // nothing needed here in this test program
    return 0;
}

static int hndlr_rotate(args_t args)
{
    // nothing needed here in this test program
    return 0;
}
