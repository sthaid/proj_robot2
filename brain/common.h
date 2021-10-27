#include <utils.h>

// brain.c ...
void brain_end_program(void);

// proc_cmd.c ...
void proc_cmd_init(void);
void proc_cmd_execute(char *transcript, double doa);
bool proc_cmd_in_progress(bool *succ);
void proc_cmd_cancel(void);

// body.c ...
void body_init(void);
int body_drive_cmd(int proc_id, int arg0, int arg1, int arg2, int arg3, char *failure_reason);
void body_emer_stop(void);
void body_power_on(void);
void body_power_off(void);

