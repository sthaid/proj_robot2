#ifdef __cplusplus
extern "C" {
#endif

int BMP280_tp_init(int dev_addr);
int BMP280_tp_read(double *temperature, double *pressure);

#ifdef __cplusplus
}
#endif

