#include <common.h>

//
// typedefs
//

typedef struct {
    char *name;
    hndlr_t proc;
} hndlr_lookup_t;

//
// variables
//

//
// prototypes
//

static int hndlr_rotate(args_t args);

//
// handlers lookup table
//

static hndlr_lookup_t tbl[] = {
    { "rotate", hndlr_rotate }
                };

// ----------------------------------------------------------------------

hndlr_t proc_cmd_lookup_hndlr(char *name)
{
    #define MAX_TBL  (sizeof(tbl)/sizeof(tbl[0]))
    int i;

    for (i = 0; i < MAX_TBL; i++) {
        if (strcmp(name, tbl[i].name) == 0) {
            return tbl[i].proc;
        }
    }

    return NULL;
}

// ----------------------------------------------------------------------

static int hndlr_rotate(args_t args)
{
    INFO("rotate: degrees=%s  dir=%s\n", args[0], args[1]);

    for (int i=0; i < 10; i++) {
        INFO("args[%d] = %s\n", i, args[i]);
    }

    return 0;
}
