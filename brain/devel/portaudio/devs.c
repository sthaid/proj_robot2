#include <stdio.h>
#include <stdlib.h>

#include <pa_utils.h>

int main(int argc, char **argv)
{
    pa_init();
    pa_print_device_info_all();
    return 0;
}
