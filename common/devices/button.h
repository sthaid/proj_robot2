#ifndef __BUTTON_H__
#define __BUTTON_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>

#define BUTTON_STATE_RELEASED 1
#define BUTTON_STATE_PRESSED  2

#define BUTTON_STATE_STR(_state) \
    ((_state) == BUTTON_STATE_PRESSED  ? "PRESSED" : \
     (_state) == BUTTON_STATE_RELEASED ? "RELEASED" : \
                                         "????")

typedef struct {
    int id;
    int state;
    int pressed_duration_us;  // only applies when state is BUTTON_STATE_RELEASED
} button_event_t;

int button_init(int max_info, ...);  // int gpio_pin, ...
void button_get_current_state(int id, int *state);
int button_get_event(button_event_t *ev);

#ifdef __cplusplus
}
#endif

#endif
