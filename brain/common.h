#include <utils.h>
#include "../body/include/body_network_intfc.h"

#define KEYID_T2S            1
#define KEYID_USER_INFO      2
#define KEYID_PROG_SETTINGS  3
#define KEYID_COLOR_ORGAN    4

struct {
    int volume;
    int brightness;
    int color_organ;
} settings;

// brain.c ...
void brain_get_recording(short *data, int max);
void brain_end_program(void);
void brain_restart_program(void);

// proc_cmd.c ...
void proc_cmd_init(void);
void proc_cmd_execute(char *transcript, double doa);
bool proc_cmd_in_progress(bool *succ);
void proc_cmd_cancel(void);

// body.c ...
void body_init(void);
int body_drive_cmd(int proc_id, int arg0, int arg1, int arg2, int arg3);
void body_emer_stop(void);
void body_power_on(void);
void body_power_off(void);
void body_status_report(void);
void body_weather_report(void);

// music.c ...
int play_music_file(char *filename);
