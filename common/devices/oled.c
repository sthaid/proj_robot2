#include <unistd.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>

#include <oled.h>
#include <SSD1306_oled.h>
#include <misc.h>

#define MAX_STR 8
#define MAX_STRLEN 8

static struct info_s {
    int dev_addr;
} info_tbl[10];
static int max_info;

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
    }
    max_info = max_info_arg;
    va_end(ap);

    // init oled device(s)
    for (id = 0; id < max_info; id++) {
        SSD1306_oled_init(info_tbl[id].dev_addr);
    }

    // set initialized flag
    initialized = true;

    // success
    return 0;
}

void oled_draw_str(int id, char *str)
{
    SSD1306_oled_drawstr(info_tbl[id].dev_addr, 0, 0, str);
}
