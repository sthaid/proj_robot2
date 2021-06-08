#include <stdio.h>
#include <stdlib.h>

#include <util_sdl.h>
#include <util_misc.h>

static int win_width = 1500;
static int win_height = 800;
static int opt_fullscreen = false;

static int pane_hndlr(pane_cx_t *pane_cx, int request, void * init_params, sdl_event_t * event);

// -----------------  MAIN  ---------------------------------------------------------

int main(int argc, char **argv)
{
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

static int pane_hndlr(pane_cx_t * pane_cx, int request, void * init_params, sdl_event_t * event)
{
    rect_t *pane = &pane_cx->pane;

    // ----------------------------
    // -------- INITIALIZE --------
    // ----------------------------

    if (request == PANE_HANDLER_REQ_INITIALIZE) {
        return PANE_HANDLER_RET_NO_ACTION;
    }

    // ------------------------
    // -------- RENDER --------
    // ------------------------

    if (request == PANE_HANDLER_REQ_RENDER) {
        sdl_render_printf(pane, 0, 0, 50, SDL_WHITE, SDL_BLUE, "Hello World!");
        return PANE_HANDLER_RET_NO_ACTION;
    }

    // -----------------------
    // -------- EVENT --------
    // -----------------------

    if (request == PANE_HANDLER_REQ_EVENT) {
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
