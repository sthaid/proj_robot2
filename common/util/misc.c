#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stddef.h>
#include <unistd.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <limits.h>
#include <assert.h>

#include <inttypes.h>
#include <sys/time.h>
#include <time.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <math.h>

#include "misc.h"

// -----------------  LOGMSG  --------------------------------------------

static void (*logmsg_cb)(char *str);

void logmsg_register_cb(void (*cb)(char *str))
{
    logmsg_cb = cb;
}

void logmsg(char *lvl, const char *func, char *fmt, ...) 
{
    va_list ap;
    char    str[1000];
    int     cnt;
    char    time_str[MAX_TIME_STR];

    // start by printing the time, lvl and func to str
    cnt = sprintf(str, "%s %s %s: ",
                  time2str(time_str, get_real_time_us(), false, true, true),
                  lvl, 
                  func);

    // construct str
    va_start(ap, fmt);
    cnt += vsnprintf(str+cnt, sizeof(str)-cnt, fmt, ap);
    va_end(ap);

    // remove terminating newline char
    if (cnt > 0 && str[cnt-1] == '\n') {
        str[cnt-1] = '\0';
        cnt--;
    }

    // log to stderr
    fprintf(stderr, "%s\n", str);

    // call callback, if registered;
    // note that the str passed to callback does not terminate in \n
    if (logmsg_cb) {
        logmsg_cb(str);  
    }
}

// -----------------  TIME ROUTINES  --------------------------------------

uint64_t microsec_timer(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC,&ts);
    return  ((uint64_t)ts.tv_sec * 1000000) + ((uint64_t)ts.tv_nsec / 1000);
}

uint64_t get_real_time_us(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_REALTIME,&ts);
    return ((uint64_t)ts.tv_sec * 1000000) + ((uint64_t)ts.tv_nsec / 1000);
}

char * time2str(char * str, int64_t us, bool gmt, bool display_ms, bool display_date) 
{
    struct tm tm;
    time_t secs;
    int32_t cnt;
    char * s = str;

    secs = us / 1000000;

    if (gmt) {
        gmtime_r(&secs, &tm);
    } else {
        localtime_r(&secs, &tm);
    }

    if (display_date) {
        cnt = sprintf(s, "%2.2d/%2.2d/%2.2d ",
                         tm.tm_mon+1, tm.tm_mday, tm.tm_year%100);
        s += cnt;
    }

    cnt = sprintf(s, "%2.2d:%2.2d:%2.2d",
                     tm.tm_hour, tm.tm_min, tm.tm_sec);
    s += cnt;

    if (display_ms) {
        cnt = sprintf(s, ".%3.3"PRId64, (us % 1000000) / 1000);
        s += cnt;
    }

    if (gmt) {
        strcpy(s, " GMT");
    }

    return str;
}

// -----------------  CONFIG READ / WRITE  -------------------------------

int config_read(char * config_path, config_t * config, int config_version)
{
    FILE * fp;
    int    i, version=0;
    char * name;
    char * value;
    char * saveptr;
    char   s[100] = "";

    // open config_file and verify version, 
    // if this fails then write the config file with default values
    if ((fp = fopen(config_path, "re")) == NULL ||
        fgets(s, sizeof(s), fp) == NULL ||
        sscanf(s, "VERSION %d", &version) != 1 ||
        version != config_version)
    {
        if (fp != NULL) {
            fclose(fp);
        }
        INFO("creating default config file %s, version=%d\n", config_path, config_version);
        return config_write(config_path, config, config_version);
    }

    // read config entries
    while (fgets(s, sizeof(s), fp) != NULL) {
        name = strtok_r(s, " \n", &saveptr);
        if (name == NULL || name[0] == '#') {
            continue;
        }

        value = strtok_r(NULL, " \n", &saveptr);
        if (value == NULL) {
            value = "";
        }

        for (i = 0; config[i].name[0]; i++) {
            if (strcmp(name, config[i].name) == 0) {
                strcpy(config[i].value, value);
                break;
            }
        }
    }

    // close
    fclose(fp);
    return 0;
}

int config_write(char * config_path, config_t * config, int config_version)
{
    FILE * fp;
    int    i;

    // open
    fp = fopen(config_path, "we");  // mode: truncate-or-create, close-on-exec
    if (fp == NULL) {
        ERROR("failed to write config file %s, %s\n", config_path, strerror(errno));
        return -1;
    }

    // write version
    fprintf(fp, "VERSION %d\n", config_version);

    // write name/value pairs
    for (i = 0; config[i].name[0]; i++) {
        fprintf(fp, "%-20s %s\n", config[i].name, config[i].value);
    }

    // close
    fclose(fp);
    return 0;
}

// -----------------  NETWORKING  ----------------------------------------

int getsockaddr(char * node, int port, struct sockaddr_in * ret_addr)
{
    struct addrinfo   hints;
    struct addrinfo * result;
    char              port_str[20];
    int               ret;

    sprintf(port_str, "%d", port);

    bzero(&hints, sizeof(hints));
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = 0;
    hints.ai_flags    = AI_NUMERICSERV;

    ret = getaddrinfo(node, port_str, &hints, &result);
    if (ret != 0) {
        ERROR("failed to get address of %s, %s\n", node, gai_strerror(ret));
        return -1;
    }
    if (result->ai_addrlen != sizeof(*ret_addr)) {
        ERROR("getaddrinfo result addrlen=%d, expected=%d\n",
            (int)result->ai_addrlen, (int)sizeof(*ret_addr));
        return -1;
    }

    *ret_addr = *(struct sockaddr_in*)result->ai_addr;
    freeaddrinfo(result);
    return 0;
}

char * sock_addr_to_str(char * s, int slen, struct sockaddr * addr)
{
    char addr_str[100];
    int port;

    if (addr->sa_family == AF_INET) {
        inet_ntop(AF_INET,
                  &((struct sockaddr_in*)addr)->sin_addr,
                  addr_str, sizeof(addr_str));
        port = ((struct sockaddr_in*)addr)->sin_port;
    } else if (addr->sa_family == AF_INET6) {
        inet_ntop(AF_INET6,
                  &((struct sockaddr_in6*)addr)->sin6_addr,
                 addr_str, sizeof(addr_str));
        port = ((struct sockaddr_in6*)addr)->sin6_port;
    } else {
        snprintf(s,slen,"Invalid AddrFamily %d", addr->sa_family);
        return s;
    }

    snprintf(s,slen,"%s:%d",addr_str,htons(port));
    return s;
}

int do_recv(int sockfd, void * recv_buff, size_t len)
{
    int ret;
    size_t len_remaining = len;

    while (len_remaining) {
        ret = recv(sockfd, recv_buff, len_remaining, MSG_WAITALL);
        if (ret <= 0) {
            if (ret == 0) {
                errno = ENODATA;
            }
            return -1;
        }

        len_remaining -= ret;
        recv_buff += ret;
    }

    return len;
}

int do_send(int sockfd, void * send_buff, size_t len)
{
    int ret;
    size_t len_remaining = len;

    while (len_remaining) {
        ret = send(sockfd, send_buff, len_remaining, MSG_NOSIGNAL);
        if (ret <= 0) {
            if (ret == 0) {
                errno = ENODATA;
            }
            return -1;
        }

        len_remaining -= ret;
        send_buff += ret;
    }

    return len;
}

// -----------------  MATH  ----------------------------------------------

double interpolate(interp_point_t *p, int n, double x)
{
    if (x < p[0].x) {
        return p[0].y;
    }
    if (x > p[n-1].x) {
        return p[n-1].y;
    }

    for (int i = 0; i < n-1; i++) {
        double min_x = p[i].x;
        double max_x = p[i+1].x;
        double min_y = p[i].y;
        double max_y = p[i+1].y;
        double interp_y;

        if (x >= min_x && x <= max_x) {
            //                      (x - min_x)
            // interp_y = min_y + --------------- * (max_y - min_y)
            //                    (max_x - min_x)
            interp_y = min_y + ((x - min_x) / (max_x - min_x)) * (max_y - min_y);
            return interp_y;
        }
    }

    FATAL("bug\n");
    return 0;
}

double sanitize_heading(double hdg, double base)
{
    if (base != 0 && base != -180) {
        FATAL("bug, invalid base %0.1f\n", base);
        return 0;
    }

    if (hdg >= base && hdg < base + 360) {
        // already okay
    } else {
        while (hdg < base) {
            hdg += 360;
        }
        while (hdg >= base + 360) {
            hdg -= 360;
        }
    }
    return hdg;
}

