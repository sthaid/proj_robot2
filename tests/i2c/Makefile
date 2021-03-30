CC       = gcc
CPPFLAGS = -Wall -g -O2 -I../../include -DTEST
LDFLAGS  = ../../devices_i2c/u8g2/libu8g2.a -lstdc++ -lm

TARGETS = MPU9250_imu \
          BMP280_tp \
          STM32_adc

all: $(TARGETS)

OBJ_MPU9250_imu = MPU9250_imu.o \
                  ../../devices_i2c/MPU9250_imu/mpu9250/MPU9250.o \
                  ../../devices_i2c/i2c/i2c.o \
                  ../../devices_i2c/i2c/I2Cdev.o \
                  ../../devices_i2c/i2c/Wire.o \
                  ../../util/misc.o
MPU9250_imu.o : CPPFLAGS += -I../../devices_i2c/MPU9250_imu
MPU9250_imu: $(OBJ_MPU9250_imu)
	$(CC) -o $@ $(OBJ_MPU9250_imu) -lm -lstdc++

OBJ_BMP280_tp = BMP280_tp.o \
                ../../devices_i2c/BMP280_tp/bmp280/BMP280.o \
                ../../devices_i2c/i2c/i2c.o \
                ../../devices_i2c/i2c/I2Cdev.o \
                ../../devices_i2c/i2c/Wire.o \
                ../../util/misc.o
BMP280_tp.o : CPPFLAGS += -I../../devices_i2c/BMP280_tp
BMP280_tp: $(OBJ_BMP280_tp)
	$(CC) -o $@ $(OBJ_BMP280_tp) -lm -lstdc++

OBJ_STM32_adc = STM32_adc.o \
                ../../devices_i2c/i2c/i2c.o \
                ../../devices_i2c/i2c/I2Cdev.o \
                ../../devices_i2c/i2c/Wire.o \
                ../../util/misc.o
STM32_adc.o : CPPFLAGS += -I../../devices_i2c/STM32_adc
STM32_adc: $(OBJ_STM32_adc)
	$(CC) -o $@ $(OBJ_STM32_adc) -lm -lstdc++
	
clean:
	rm -f $(TARGETS) *.o
	find ../../devices_i2c/ -name \*.o -exec rm {} \;
	find ../../util/ -name \*.o -exec rm {} \;

# xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx

OLD_SRC      = \
           ../../devices_i2c/MCP9808_temp/MCP9808_temp.c \
           ../../devices_i2c/SSD1306_oled/SSD1306_oled.c \
           ../../devices_i2c/BME680_tphg/BME680_tphg.c \
           ../../devices_i2c/BME680_tphg/bme680/bme680.cpp \
           ../../devices_i2c/i2c/i2c.c \
           ../../devices_i2c/i2c/I2Cdev.cpp \
           ../../devices_i2c/i2c/Wire.cpp \
           ../../util/misc.c

