#ifndef __MCP9808_TEMP_H__
#define __MCP9808_TEMP_H__

#ifdef __cplusplus
extern "C" {
#endif

int MCP9808_temp_init(int dev_addr);
int MCP9808_temp_read(double *degc);

#ifdef __cplusplus
}
#endif

#endif
