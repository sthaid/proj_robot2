#include <utils.h>

// defines
#define MAX_SV 1000000

#define NO_LIVECAPTION   // use during development to not run livecaption

// variables
static char    * transcript;
static pthread_t s2t_tid;
static short     sv[MAX_SV];
static int       max_sv;
static bool      terminating;

// prototypes
static void s2t_exit(void);
static void *s2t_thread(void *cx);

// -----------------  INIT AND EXIT  --------------------------------------------

void s2t_init(void)
{
    // create s2t_thread
    pthread_create(&s2t_tid, NULL, s2t_thread, NULL);

    // atexit
    atexit(s2t_exit);
}

static void s2t_exit(void)
{
    // set terminating flag, so that the s2t_thread will terminate
    terminating = true;

    // wait for s2t_thread to exit
    pthread_join(s2t_tid, NULL);

    // be extra sure that the livecaption program is not running
    // xxx confirm livecaption is in 'ps'
    system("killall livecaption");
}

// -----------------  FEED  -----------------------------------------------------

// caller must free returned transcript
char * s2t_feed(short sound_val)
{
    if (transcript != NULL) {
        char *ts = transcript;
        max_sv = 0;
        __sync_synchronize();
        transcript = NULL;
        return ts;
    }

    sv[max_sv++] = sound_val;
    return NULL;
}

// -----------------  THREAD  ---------------------------------------------------

#ifndef NO_LIVECAPTION

static void *s2t_thread(void *cx)
{
    #define MAX_TS 4096

    uint64_t start_time;
    char    *ts;
    int      rc, fd_to_lc, fd_from_lc, flags, avail, sv_idx;
    pid_t    lc_pid;

    while (true) {
        // wait for indication to start 
        while (max_sv == 0) {
            if (terminating) return NULL;
            usleep(30000);
        }

        // execute the livecaption go program, and
        // set fd used to read livecaption stdout to non blocking
        run_program(&lc_pid, &fd_to_lc, &fd_from_lc, "./go/livecaption", NULL);
        flags = fcntl(fd_from_lc, F_GETFL, 0); 
        flags |= O_NONBLOCK; 
        fcntl(fd_from_lc, F_SETFL, flags); 

        // int variables
        ts = calloc(MAX_TS,1);
        strcpy(ts, "TIMEDOUT");
        start_time = microsec_timer();
        sv_idx = 0;

        while (true) {
            // break out of this loop if terminate is requested
            if (terminating) break;

            // if more than 100 new sound values are available then 
            // provide these new sound values to livecaption pgm
            avail = max_sv - sv_idx - 1;
            if (avail > 100) {
                rc = write(fd_to_lc, sv+sv_idx, avail*sizeof(short));
                if ((rc != avail*sizeof(short)) && (rc != -1 || errno != EPIPE)) {
                    ERROR("failed write to livecaption, rc=%d, %s\n", rc, strerror(errno));
                    break;
                }
                sv_idx += avail;
            }
            

            // perform non blocking read of livecaption stdout
            rc = read(fd_from_lc, ts, MAX_TS-1);
            if (rc < 0 && errno != EWOULDBLOCK) {
                ERROR("failed read from livecaption, %s\n", strerror(errno));
                break;
            }

            // if we have a result from livecaption then break
            if (rc > 0) {
                break;
            }

            // timeout if there is not a result from livecaption in 10 secs
            // xxx make this an arg
            if (microsec_timer() - start_time > 10000000) {
                WARN("timedout waiting for transcript from livecaption\n");
                break;
            }

            // short sleep, 10ms
            usleep(10000);
        }

        // the transcript is ready to be returned by s2t_feed
        ts[strcspn(ts, "\n")] = '\0';
        INFO("TRANSCRIPT: '%s'\n", ts);
        transcript = ts;

        // close fds and call waitpid
        // xxx how long does this take
        close(fd_to_lc);
        close(fd_from_lc);
        waitpid(lc_pid, NULL, 0);

        // wait for s2t_feed to acknowledge that it has the transcript
        while (transcript) {
            if (terminating) return NULL;
            usleep(10000);
        }
    }

    return NULL;
}

#else

static void *s2t_thread(void *cx)
{
    char *ts;

    while (true) {
        // wait for indication to start 
        while (max_sv == 0) {
            if (terminating) return NULL;
            usleep(30000);
        }

        // provide a dummy transcript
        sleep(1);
        ts = malloc(100);
        strcpy(ts, "dummy transcript");
        INFO("TRANSCRIPT: '%s'\n", ts);
        transcript = ts;

        // wait for s2t_feed to acknowledge that it has the transcript
        while (transcript) {
            if (terminating) return NULL;
            usleep(10000);
        }
    }

    return NULL;
}

#endif
