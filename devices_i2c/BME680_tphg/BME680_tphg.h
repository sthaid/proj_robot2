int BME680_tphg_init(int dev_addr);
int BME680_tphg_read(int dev_addr, double *temperature, double *pressure, 
                     double *humidity, double *gas_resistance);
