#include <stdio.h>
#include <unistd.h>

#include <body.h>
#include <env.h>

#define PASCAL_TO_INHG(pa)  ((pa) * 0.0002953)

int main(int argc, char **argv)
{
    double temperature, pressure;

    if (env_init(0)) {
        printf("env_init failed\n");
        return 1;
    }

    while (1) {
        sleep(1);
        env_read(&temperature, &pressure);
        printf("temp=%0.1f C  pressure=%0.0f Pascal (%0.2f in Hg)\n", 
              temperature, pressure, PASCAL_TO_INHG(pressure));
    }

    return 0;
}
