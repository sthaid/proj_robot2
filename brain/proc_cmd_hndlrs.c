#include <common.h>

//
// typedefs
//

typedef struct {
    char *name;
    proc_cmd_hndlr_t proc;
} hndlr_t;

//
// variables
//

//
// prototypes
//

static int hndlr_rotate(int argc, char **argv);

//
// handlers lookup table
//

static hndlr_t tbl[] = {
    { "rotate", hndlr_rotate }
                };

// ----------------------------------------------------------------------

proc_cmd_hndlr_t proc_cmd_lookup_hndlr(char *name)
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

static int hndlr_rotate(int argc, char **argv)
{
    assert(argc == 2);

    INFO("rotate: degrees=%s  dir=%s\n", argv[0], argv[1]);

    return 0;
}
