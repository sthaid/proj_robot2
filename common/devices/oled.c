#include <unistd.h>
#include <stdbool.h>
#include <stdarg.h>
#include <pthread.h>
#include <string.h>

#include <oled.h>
#include <SSD1306_oled.h>
#include <misc.h>

#define MAX_STR 8
#define MAX_STRLEN 20

static struct info_s {
    int dev_addr;
    pthread_t tid;
    char str[MAX_STR][MAX_STRLEN];
    int stridx;
    bool next;
    unsigned int intvl_us;
} info_tbl[10];
static int max_info;

static void * oled_thread(void *cx);
static void set_next_stridx(struct info_s *x);

// -----------------  API  --------------------------------------

int oled_init(int max_info_arg, ...)   // int dev_addr, ...
{
    static bool initialized;
    int id;
    va_list ap;

    // check if already initialized
    if (initialized) {
        ERROR("already initialized\n");
        return -1;
    }

    // save oled dev_addr in info_tbl
    va_start(ap, max_info_arg);
    for (id = 0; id < max_info_arg; id++) {
        info_tbl[id].dev_addr = va_arg(ap, int);
        info_tbl[id].intvl_us = 1000000;
    }
    max_info = max_info_arg;
    va_end(ap);

    // init oled device(s)
    for (id = 0; id < max_info; id++) {
        SSD1306_oled_init(info_tbl[id].dev_addr);
    }

    // create threads, one for each oled device
    for (id = 0; id < max_info; id++) {
        pthread_create(&info_tbl[id].tid, NULL, oled_thread, &info_tbl[id]);
    }

    // set initialized flag
    initialized = true;

    // success
    return 0;
}

void oled_set_str(int id, int stridx, char *str)
{
    strncpy(info_tbl[id].str[stridx], str, MAX_STRLEN);
    info_tbl[id].str[stridx][MAX_STRLEN-1] = '\0';
}

void oled_set_intvl_us(int id, unsigned int intvl_us)
{
    info_tbl[id].intvl_us = intvl_us;
}

void oled_set_next(int id)
{
    info_tbl[id].next = true;
}

// -----------------  THREAD-------------------------------------

static void * oled_thread(void *cx)
{
    struct info_s *x = cx;
    uint64_t start_us;

    while (true) {
        // draw str[stridx]
        SSD1306_oled_drawstr(x->dev_addr, 0, 0, x->str[x->stridx]);

        // wait for either intvl_us or next flag
        start_us = microsec_timer();
        while (true) {
            if ((x->intvl_us != 0 && microsec_timer() - start_us > x->intvl_us) || x->next) {
                x->next = false;
                break;
            }
            usleep(100000);  // 100 ms
        }

        // set stridx for next string
        set_next_stridx(x);
    }

    return NULL;
}

static void set_next_stridx(struct info_s *x)
{
    int new_stridx, i;

    for (i = 1; i <= MAX_STR; i++) {
        new_stridx = (x->stridx + i) % MAX_STR;
        if (x->str[new_stridx][0] != '\0') {
            break;
        }
    }

    x->stridx = new_stridx;
}
