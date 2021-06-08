#include <stdio.h>
#include <stdlib.h>

#include <portaudio.h>
#include <pa_utils.h>

int main(int argc, char **argv)
{
    Pa_Initialize();
    pa_print_device_info_all();
    Pa_Terminate();
    return 0;
}
