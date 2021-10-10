// gcc -Wall -g -I. grammar_test.c grammar.c misc.c -lm -o gt

#include <utils.h>

static int hndlr_stub(args_t args);

static hndlr_lookup_t hlu[] = {
    { "test",   hndlr_stub },
    { "rotate", hndlr_stub },
    { "name", hndlr_stub },
    { NULL, NULL } };

struct test_s {
    char *cmd;
    bool  ok;
    char *arg0;
    char *arg1;
} test[] = {
    // match okay
    { "rotate 45 degrees",              true, "45 degrees", "" },
    { "rotate 45 degrees ccw",          true, "45 degrees", "ccw" },
    { "turn",                           true, "", "" },
    { "turn cw half way around",        true, "half way around", "cw" },
    { "turn ccw all the way around",    true, "all the way around", "ccw" },
    { "turn ccw all of the way around", true, "all of the way around", "ccw" },
    { "test1 next opt2 eol",            true, "opt2", "eol" },
    { "test1 next opt2",                true, "opt2", "" },
    { "test1",                          true, "", "" },
    { "test1 eol",                      true, "", "eol" },
    { "test1 next",                     true, "", "" },
    { "test1 opt 3 and 4",              true, "opt 3 and 4", "" },
    { "test1 opt1",                     true, "opt1", "" },
    { "test1 next opt1 eol",            true, "opt1", "eol" },
    { "test2 next",                     true, "", "" },
    { "test3 next",                     true, "next",    "" },
    { "my name is first last eol",      true, "first", "last" },
    // match not okay
    { "hello world",                    false, "", "" },
    { "test1 hello opt1 eol",           false, "", "" },
    { "test1 optx eol",                 false, "", "" },
    { "test1 opt1 eol toomuch",         false, "", "" },
    { "test2",                          false, "", "" },
    { "test3",                          false, "", "" },
    { "",                               false, "", "" },
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

    logging_init(NULL,false);  // use stdout for log msgs

    rc = grammar_init("grammar_test.syntax", hlu);
    if (rc < 0) {
        FATAL("grammar_init failed\n");
    }

    for (i = 0; i < MAX_TEST; i++) {
        struct test_s *t = &test[i];
        ok = grammar_match(t->cmd, &hndlr, args);

        if (ok == t->ok && 
            strcmp(args[0], t->arg0) == 0 &&
            strcmp(args[1], t->arg1) == 0) 
        {
            //printf("TEST %d: OKAY\n", i);
        } else {
            printf("TEST %d: FAILED\n", i);
            printf("cmd   = '%s'\n", t->cmd);
            printf("match = %s\n", (ok ? "true" : "false"));
            printf("arg0  = expected:'%s' actual:'%s'\n", t->arg0, args[0]);
            printf("arg1  = expected:'%s' actual:'%s'\n", t->arg1, args[1]);
            exit(1);
        }
    }
    printf("ALL TESTS PASSED\n");
    printf("\n");

    #define CYCLES 10000
    printf("TIMING: COMPLEX MATCH ...\n");
    uint64_t start, duration;
    start = microsec_timer();
    for (i = 0; i < CYCLES; i++) {
        ok = grammar_match("turn ccw all of the way around", &hndlr, args);
        if (!ok) {
            printf("BUG\n");
            exit(1);
        }
    }
    duration = microsec_timer() - start;
    printf("time = %0.3f usecs\n\n", (double)duration / CYCLES);

    printf("TIMING: SIMPLE NOT-A-MATCH ...\n");
    start = microsec_timer();
    for (i = 0; i < CYCLES; i++) {
        ok = grammar_match("notamatch", &hndlr, args);
        if (ok) {
            printf("BUG\n");
            exit(1);
        }
    }
    duration = microsec_timer() - start;
    printf("time = %0.3f usecs\n\n", (double)duration / CYCLES);

    return 0;
}

// -----------------  HANDLERS  ---------------------------------

static int hndlr_stub(args_t args)
{
    // nothing needed here in this test program
    return 0;
}

