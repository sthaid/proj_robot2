#include <utils.h>

int main(int argc, char **argv)
{
    char *file_name = (argc > 1 ? argv[1] : "db.dat");

    log_init(NULL, false, true);
    misc_init();
    db_init(file_name, false, 0);

    db_dump();

    return 0;
}
