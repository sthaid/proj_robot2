#include <utils.h>
  
// ----------------------------------------------------------------------

void misc_init(void)
{
    // nothing needed yet
}

// ----------------------------------------------------------------------

uint64_t microsec_timer(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC,&ts);
    return  ((uint64_t)ts.tv_sec * 1000000) + ((uint64_t)ts.tv_nsec / 1000);
}

char *time2str(time_t t, char *s)
{
    struct tm result;

    localtime_r(&t, &result);
    sprintf(s, "%4d/%02d/%02d %02d:%02d:%02d",
            result.tm_year+1900, result.tm_mon+1, result.tm_mday,
            result.tm_hour, result.tm_min, result.tm_sec);

    return s;
}

// ported from http://www.noah.org/wiki/Wavelength_to_RGB_in_Python
unsigned int wavelen_to_rgb(double wavelength)
{
    double attenuation;
    double gamma = 0.8;
    double R,G,B;

    if (wavelength >= 380 && wavelength <= 440) {
        double attenuation = 0.3 + 0.7 * (wavelength - 380) / (440 - 380);
        R = pow((-(wavelength - 440) / (440 - 380)) * attenuation, gamma);
        G = 0.0;
        B = pow(1.0 * attenuation, gamma);
    } else if (wavelength >= 440 && wavelength <= 490) {
        R = 0.0;
        G = pow((wavelength - 440) / (490 - 440), gamma);
        B = 1.0;
    } else if (wavelength >= 490 && wavelength <= 510) {
        R = 0.0;
        G = 1.0;
        B = pow(-(wavelength - 510) / (510 - 490), gamma);
    } else if (wavelength >= 510 && wavelength <= 580) {
        R = pow((wavelength - 510) / (580 - 510), gamma);
        G = 1.0;
        B = 0.0;
    } else if (wavelength >= 580 && wavelength <= 645) {
        R = 1.0;
        G = pow(-(wavelength - 645) / (645 - 580), gamma);
        B = 0.0;
    } else if (wavelength >= 645 && wavelength <= 750) {
        attenuation = 0.3 + 0.7 * (750 - wavelength) / (750 - 645);
        R = pow(1.0 * attenuation, gamma);
        G = 0.0;
        B = 0.0;
    } else {
        R = 0.0;
        G = 0.0;
        B = 0.0;
    }

    if (R < 0) R = 0; else if (R > 1) R = 1;
    if (G < 0) G = 0; else if (G > 1) G = 1;
    if (B < 0) B = 0; else if (B > 1) B = 1;

    return ((int)nearbyint(R*255) <<  0) |
           ((int)nearbyint(G*255) <<  8) |
           ((int)nearbyint(B*255) << 16);
}

// run program using fork and exec
void run_program(pid_t *prog_pid, int *fd_to_prog, int *fd_from_prog, char *prog, ...)
{
    char *args[100];
    int argc=0;
    int pipe_to_child[2], pipe_from_child[2];
    pid_t pid;
    va_list ap;
    sigset_t set;

    // block SIGPIPE
    sigemptyset(&set);
    sigaddset(&set,SIGPIPE);
    sigprocmask(SIG_BLOCK, &set, NULL);

    // construct args array
    args[argc++] = prog;
    va_start(ap, prog);
    while (true) {
        args[argc] = va_arg(ap, char*);
        if (args[argc] == NULL) break;
        argc++;
    }
    va_end(ap);

    // create pipes for prog input and output
    // - pipefd[0] is read end, pipefd[1] is write end
    pipe(pipe_to_child);
    pipe(pipe_from_child);

    // fork
    pid = fork();
    if (pid == -1) {
        printf("ERROR: fork failed, %s\n", strerror(errno));
        exit(1);
    }

    // if pid == 0, child is running, else parent
    if (pid == 0) {
        // child ..
        // close unused ends of the pipes
        close(pipe_to_child[1]);
        close(pipe_from_child[0]);

        // attach the 2 pipes to stdin and stdout for the child
        dup2(pipe_to_child[0], 0);
        dup2(pipe_from_child[1], 1);

        // execute the program
        execvp(prog, args);
        printf("ERROR: execvp %s, %s\n", prog, strerror(errno));
        exit(1);        
    } else {
        // parent ... 
        // close unused ends of the pipes
        close(pipe_to_child[0]);
        close(pipe_from_child[1]);

        // return values to caller
        *fd_to_prog = pipe_to_child[1];;
        *fd_from_prog = pipe_from_child[0];
        *prog_pid = pid;
    }
}
