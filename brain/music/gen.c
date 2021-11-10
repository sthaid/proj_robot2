// gcc -g -O2 -Wall -I../utils gen.c ../utils/sf.c ../utils/logging.c ../utils/misc.c -lm -lsndfile -o gen

#include <utils.h>

int main(int argc, char **argv)
{
    log_init(NULL,false,true);

    sf_gen_sweep_wav("frequency_sweep.wav",
                     100, 8000,   // freq range
                     30,          // duration secs
                     1, 48000);   // max_chan, sample_rate

    sf_gen_white_wav("white_noise.wav",
                     30,          // duration secs
                     1, 48000);   // max_chan, sample_rate

    return 0;
}
