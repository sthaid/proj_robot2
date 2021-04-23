#ifndef __BUTTON_H__
#define __BUTTON_H__

#ifdef __cplusplus
extern "C" {
#endif

// Notes:
// - button_init varargs: int gpio_pin, ...

typedef void (*button_cb_t)(int id, bool pressed, int pressed_duration_us);

int button_init(int max_info, ...);        // returns -1 on error, else 0
void button_register_cb(int id, button_cb_t cb);
bool button_is_pressed(int id);

#ifdef __cplusplus
}
#endif

#endif
