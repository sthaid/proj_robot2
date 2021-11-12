#include <utils.h>

static char *file_name = "db.dat";

static void usage(void);

int main(int argc, char **argv)
{
    int rc, keyid;
    char *keyidstr, *keystr;

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

    // get the keyidstr and keystr
    keyidstr = argv[optind];
    keystr = argv[optind+1];

    // verify
    if (keyidstr == NULL || keystr == NULL || sscanf(keyidstr, "%d", &keyid) != 1) {
        usage();
        return 1;
    }

    // open database
    INFO("OPENING %s\n", file_name);
    db_init(file_name, false, 0);
    
    // remove the requested entry
    INFO("REMOVING %d '%s'\n", keyid, keystr);
    rc = db_rm(keyid, keystr);
    if (rc != 0) {
        ERROR("FAILED TO REMOVE %d %s\n", keyid, keystr);
        return 1;
    }

    // success
    return 0;
}

static void usage(void)
{
    ERROR("usage: db_rm [-f db_file] keyid keystr\n");
}
