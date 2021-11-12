#include <utils.h>

char *file_name = "db.dat";

static void usage(void);

int main(int argc, char **argv)
{
    // init logging
    log_init(NULL, false, true);
    misc_init();

    // get and process options
    while (true) {
        signed char opt_char = getopt(argc, argv, "f:h");
        if (opt_char == -1) {
            break;
        }
        switch (opt_char) {
        case 'f':
            file_name = optarg;
            break;
        case 'h':
            usage();
            return 1;
        default:
            return 1;
            break;
        }
    }

    // open database
    INFO("OPENING %s\n\n", file_name);
    db_init(file_name, false, 0);

    // dump database
    INFO("DUMPING ...\n\n");
    db_dump();

    // done
    return 0;
}

static void usage(void)
{
    ERROR("usage: db_dump [-f db_file]\n");
}

