#include <utils.h>

void proc_cmd_init(void);
void proc_cmd_exit(void);

void proc_cmd_execute(char *transcript, double doa);
bool proc_cmd_in_progress(void);
void proc_cmd_cancel(void);

