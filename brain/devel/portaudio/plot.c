#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <math.h>

#include <util_sdl.h>
#include <util_misc.h>
#include <pa_utils.h>
#include <file_utils.h>

//
// defines
//

// used when data being obtained from microphone
#define SEEED_4MIC_VOICECARD "seeed-4mic-voicecard"
#define MAX_CHAN             4
#define SAMPLE_RATE          48000  // samples per sec
#define DURATION             5      // secs

#define DATA_SRC_MIC  1
#define DATA_SRC_FILE 2

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
static bool   chan_data_ready;

static int    data_src;
static char  *data_src_name;

//
// prototypes
//

static void *get_mic_data_thread(void *cx);
static int pane_hndlr(pane_cx_t *pane_cx, int request, void * init_params, sdl_event_t * event);

// -----------------  MAIN  ---------------------------------------------------------

int main(int argc, char **argv)
{
    pthread_t tid;

    // set default data source
    data_src_name = SEEED_4MIC_VOICECARD;
    data_src = DATA_SRC_MIC;

    // parse options
    // -f <file_name> : get audio data from file
    // -d <dev_name>  : get audio data from microphone
    // -h             : help
    while (true) {
        int ch = getopt(argc, argv, "f:d:h");
        if (ch == -1) {
            break;
        }
        switch (ch) {
        case 'f':
            data_src_name = optarg;
            data_src = DATA_SRC_FILE;
            break;
        case 'd':
            data_src_name = optarg;
            data_src = DATA_SRC_MIC;
            break;
        case 'h':
            printf("usage: plot [-d indev] [-f filename]\n");
            return 0;
            break;
        default:
            return 1;
        };
    }

    // get audio data eitehr from microphone or file
    if (data_src == DATA_SRC_MIC) {
        if (pa_init() < 0) {
            FATAL("failed to initialize portaudio\n");
        }
        max_chan    = MAX_CHAN;
        max_data    = DURATION * SAMPLE_RATE;
        sample_rate = SAMPLE_RATE;
        for (int chan = 0; chan < max_chan; chan++) {
            chan_data[chan] = malloc(max_data * sizeof(float));
        }
        pthread_create(&tid, NULL, get_mic_data_thread, NULL);
    } else {
        if (file_read(data_src_name, &max_chan, &max_data, &sample_rate, chan_data) < 0) {
            FATAL("failed to read file %s\n", data_src_name);
        }
        chan_data_ready = true;
    }

    // print info
    INFO("%s %s: max_chan=%d max_data=%d sample_rate=%d duration=%0.1f \n",
         (data_src == DATA_SRC_MIC ? "mic" : "file"),
         data_src_name,
         max_chan,
         max_data,
         sample_rate,
         (double)max_data / sample_rate);

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

static void *get_mic_data_thread(void *cx)
{
    if (pa_record(data_src_name, max_chan, max_data, sample_rate, chan_data) < 0) {
        FATAL("failed pa_record, %s\n", data_src_name);
    }
    chan_data_ready = true;
    return NULL;
}

// -----------------  PANE HNDLR  ---------------------------------------------------

#define FOOTER_HEIGHT 50

static int ctr_smpl;
static int cursor_x;
static int pxls_per_smpl;

static void plot(rect_t *pane, int chan);

static int pane_hndlr(pane_cx_t * pane_cx, int request, void * init_params, sdl_event_t * event)
{
    rect_t *pane = &pane_cx->pane;

    // ----------------------------
    // -------- INITIALIZE --------
    // ----------------------------

    if (request == PANE_HANDLER_REQ_INITIALIZE) {
        ctr_smpl      = 0;
        cursor_x      = pane->w / 2;
        pxls_per_smpl = 1;
        return PANE_HANDLER_RET_NO_ACTION;
    }

    // ------------------------
    // -------- RENDER --------
    // ------------------------

    if (request == PANE_HANDLER_REQ_RENDER) {
        if (chan_data_ready) {
            // plot the channel data
            for (int chan = 0; chan < max_chan; chan++) {
                plot(pane, chan); 
            }

            // place label info at the bottom 
            double ctr_time = (double)ctr_smpl / SAMPLE_RATE;
            double tspan    = (double)pane->w / pxls_per_smpl / SAMPLE_RATE;
            int sample_span = pane->w / pxls_per_smpl;  // XXX should this be double, maybe delete 
            char str[50];

            sprintf(str, "%0.3f  %d", ctr_time-tspan/2, ctr_smpl-sample_span/2);
            sdl_render_printf(pane, 
                              0, 
                              pane->h - ROW2Y(2,25), 
                              25, SDL_WHITE, SDL_BLACK, "%s", str);

            sprintf(str, "%0.3f  %d", ctr_time, ctr_smpl);
            sdl_render_printf(pane, 
                              pane->w/2 - COL2X(strlen(str)/2.,25),
                              pane->h - ROW2Y(2,25), 
                              25, SDL_WHITE, SDL_BLACK, "%s", str);

            sprintf(str, "%d  %0.3f", ctr_smpl+sample_span/2, ctr_time+tspan/2);
            sdl_render_printf(pane, 
                              pane->w - COL2X(strlen(str),25),
                              pane->h - ROW2Y(2,25), 
                              25, SDL_WHITE, SDL_BLACK, "%s", str);

            sprintf(str, "TSPAN=%0.3f   ZOOM=%d", tspan, pxls_per_smpl);
            sdl_render_printf(pane, 
                              pane->w/2 - COL2X(strlen(str)/2.,25),
                              pane->h - ROW2Y(1,25), 
                              25, SDL_WHITE, SDL_BLACK, "%s", str);
            
            // draw cursor
            sdl_render_line(pane, cursor_x, 0, cursor_x, pane->h-FOOTER_HEIGHT, SDL_GREEN);
        } else {
            // print 'NO DATA'
            sdl_render_printf(pane, 
                              pane->w/2 - 3.5 * sdl_font_char_width(50),
                              pane->h/2 - 0.5 * sdl_font_char_height(50),
                              50, SDL_WHITE, SDL_BLACK, "NO DATA");
        }
        return PANE_HANDLER_RET_NO_ACTION;
    }

    // -----------------------
    // -------- EVENT --------
    // -----------------------

    if (request == PANE_HANDLER_REQ_EVENT) {
        switch (event->event_id) {
        case SDL_EVENT_KEY_LEFT_ARROW:
            ctr_smpl--;
            break;
        case SDL_EVENT_KEY_RIGHT_ARROW:
            ctr_smpl++;
            break;
        case SDL_EVENT_KEY_LEFT_ARROW | SDL_EVENT_KEY_CTRL:
            ctr_smpl -= 20;
            break;
        case SDL_EVENT_KEY_RIGHT_ARROW | SDL_EVENT_KEY_CTRL:
            ctr_smpl += 20;
            break;
        case SDL_EVENT_KEY_LEFT_ARROW | SDL_EVENT_KEY_ALT: {
            double ctr_time = (double)ctr_smpl / SAMPLE_RATE;
            ctr_smpl = nearbyint(ctr_time * 10 - 1) / 10. * SAMPLE_RATE;
            break; }
        case SDL_EVENT_KEY_RIGHT_ARROW | SDL_EVENT_KEY_ALT: {
            double ctr_time = (double)ctr_smpl / SAMPLE_RATE;
            ctr_smpl = nearbyint(ctr_time * 10 + 1) / 10. * SAMPLE_RATE;
            break; }
        case 'Z':
            pxls_per_smpl++;
            break;
        case 'z':
            pxls_per_smpl--;
            break;
        case '<':
            cursor_x--;
            break;
        case '>':
            cursor_x++;
            break;
        case 'g':
            if (data_src == DATA_SRC_MIC && chan_data_ready) {
                pthread_t tid;
                chan_data_ready = false;
                pthread_create(&tid, NULL, get_mic_data_thread, NULL);
            }
            break;
        default:
            break;
        }

        if (ctr_smpl < 0) ctr_smpl = 0;
        if (ctr_smpl >= max_data) ctr_smpl = max_data-1;
        if (pxls_per_smpl > 10) pxls_per_smpl = 10;
        if (pxls_per_smpl < 1) pxls_per_smpl = 1;
        if (cursor_x < 0) cursor_x = 0;
        if (cursor_x >= pane->w) cursor_x = pane->w-1;

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
    point_t p[10000];
    int     max_points = 0;
    int     y_origin;
    int     pxl;
    int     smpl;

    y_origin = ((pane->h-FOOTER_HEIGHT) / 4) * (0.5 + chan);

    smpl = ctr_smpl - (pane->w/2) / pxls_per_smpl;
    pxl  = pane->w/2 - (ctr_smpl - smpl) * pxls_per_smpl;

    if (pxl < 0) {
        FATAL("pxl=%d\n", pxl);
    }

    sdl_render_line(pane, 0, y_origin, pane->w-1, y_origin, SDL_GREEN);

    while (true) {
        if (smpl >= 0 && smpl < max_data) {
            p[max_points].x = pxl;
            p[max_points].y = y_origin + (chan_data[chan][smpl] * ((pane->h-FOOTER_HEIGHT)/8));
            max_points++;
        }
        smpl++;
        pxl += pxls_per_smpl;

        if (pxl >= pane->w) {
            break;
        }
    }

    sdl_render_lines(pane, p, max_points, SDL_WHITE);
}

