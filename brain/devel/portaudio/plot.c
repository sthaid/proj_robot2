#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <util_sdl.h>
#include <util_misc.h>
#include <pa_utils.h>
#include <file_utils.h>

//
// defines
//

//
// variables
//

static int    win_width = 1500;
static int    win_height = 800;
static int    opt_fullscreen = false;

static int    max_chan;
static int    max_data;
static int    sample_rate;
static float *chan_data[32];  // up to 32 channels

//
// prototypes
//

static int pane_hndlr(pane_cx_t *pane_cx, int request, void * init_params, sdl_event_t * event);

// -----------------  MAIN  ---------------------------------------------------------

int main(int argc, char **argv)
{
    char *file_name = "record.dat";
    int rc;

    // xxx also get mic data directly, and specify a duration
    rc = file_read(file_name, &max_chan, &max_data, &sample_rate, chan_data);
    if (rc < 0) {
	FATAL("file_read failed\n");
    }
    printf("file_read returned max_chan=%d max_data=%d sample_rate=%d\n",
           max_chan, max_data, sample_rate);

    // init sdl
    if (sdl_init(&win_width, &win_height, opt_fullscreen, false, false) < 0) {
        FATAL("sdl_init %dx%d failed\n", win_width, win_height);
    }

    // run the pane manger, this is the runtime loop
    sdl_pane_manager(
        NULL,           // context
        NULL,           // called prior to pane handlers
        NULL,           // called after pane handlers
        10000,          // 0=continuous, -1=never, else us
        1,              // number of pane handler varargs that follow
        pane_hndlr, NULL, 0, 0, win_width, win_height, PANE_BORDER_STYLE_NONE);

    // program terminating
    INFO("TERMINATING\n");
    return 0;
}

// -----------------  PANE HNDLR  ---------------------------------------------------

static int x_start = 10000;
static int x_cursor;

static void plot(rect_t *pane, int chan);

static int pane_hndlr(pane_cx_t * pane_cx, int request, void * init_params, sdl_event_t * event)
{
    rect_t *pane = &pane_cx->pane;

    // ----------------------------
    // -------- INITIALIZE --------
    // ----------------------------

    if (request == PANE_HANDLER_REQ_INITIALIZE) {
	x_cursor = pane->w / 2;
        return PANE_HANDLER_RET_NO_ACTION;
    }

    // ------------------------
    // -------- RENDER --------
    // ------------------------

    if (request == PANE_HANDLER_REQ_RENDER) {
        for (int chan = 0; chan < max_chan; chan++) {
            plot(pane, chan); 
        }

	sdl_render_line(pane, x_cursor, 0, x_cursor, pane->h-1, SDL_WHITE);

        return PANE_HANDLER_RET_NO_ACTION;
    }

    // -----------------------
    // -------- EVENT --------
    // -----------------------

    if (request == PANE_HANDLER_REQ_EVENT) {
        switch (event->event_id) {
	case SDL_EVENT_KEY_LEFT_ARROW:
	    x_start++;
	    break;
	case SDL_EVENT_KEY_RIGHT_ARROW:
	    x_start--;
	    break;
	case '<':
	    x_cursor++;
	    break;
	case '>':
	    x_cursor--;
	    break;
        default:
            INFO("got event_id 0x%x\n", event->event_id);
            break;
        }
//xxx get the data       g
//xxx scroll x           left right arrows     change start sample
//xxx change x scale     + -                   change sample_per_pixel
//xxx change y scale     shift + -             change maxy

        return PANE_HANDLER_RET_NO_ACTION;
    }

    // ---------------------------
    // -------- TERMINATE --------
    // ---------------------------

    if (request == PANE_HANDLER_REQ_TERMINATE) {
        return PANE_HANDLER_RET_NO_ACTION;
    }

    // not reached
    FATAL("not reached\n");
    return PANE_HANDLER_RET_NO_ACTION;
}

static void plot(rect_t *pane, int chan)
{
    int max_p = 0;
    int x, y_base;
    point_t p[10000];
    int x_data_idx;

    y_base = (pane->h / 4) * (0.5 + chan);
    //INFO("chan=%d y_base=%d\n", chan, y_base);

    x = 0;
    for (x_data_idx = x_start; true; x_data_idx++) {
        p[max_p].x = x;
        p[max_p].y = y_base + chan_data[chan][x_data_idx] * (pane->h/8);
        max_p++;
	x += 10;   // XXX
	if (x >= pane->w) {
	    break;
	}
    }

    sdl_render_line(pane, 0, y_base, pane->w-1, y_base, SDL_WHITE);
    sdl_render_lines(pane, p, max_p, SDL_WHITE);
}

