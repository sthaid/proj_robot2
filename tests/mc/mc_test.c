#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <mc.h>
#include <misc.h>

int main(int argc, char **argv)
{
    int id, error_status;

    if (mc_init() < 0) exit(1);

    while (true) {
        for (id = 0; id < MAX_MC; id++) {
            error_status = -1;
            mc_status(id, &error_status);
            printf("ID %d  ERROR_STATUS 0x%x\n", id, error_status);
            sleep(1);
        }
    }

    return 0;
}
