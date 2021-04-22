#ifndef __BUTTON_H__
#define __BUTTON_H__

#ifdef __cplusplus
extern "C" {
#endif

#define BUTTON_STATE_RELEASED 0
#define BUTTON_STATE_PRESSED  1

#define BUTTON_STATE_STR(_state) \
    ((_state) == BUTTON_STATE_PRESSED  ? "PRESSED" : \
     (_state) == BUTTON_STATE_RELEASED ? "RELEASED" : \
                                         "????")

typedef struct {
    int id;
    int state;
    int pressed_duration_us;  // only applies when state is BUTTON_STATE_RELEASED
} button_event_t;

// Notes:
// - button_init varargs: int gpio_pin, ...

int button_init(int max_info, ...);        // returns -1 on error, else 0
int button_get_event(button_event_t *ev);  // returns -1 if no event, else return 0 and *ev
int button_get_current_state(int id);      // returns current button state

#ifdef __cplusplus
}
#endif

#endif
