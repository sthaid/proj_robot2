#include <utils.h>

// brain.c ...
void brain_end_program(void);

// proc_cmd.c ...
void proc_cmd_init(void);
void proc_cmd_execute(char *transcript, double doa);
bool proc_cmd_in_progress(void);
void proc_cmd_cancel(void);

// body_intfc.c ...
void body_intfc_init(void);
int body_intfc_send_msg(int id, void *data, int data_len);
