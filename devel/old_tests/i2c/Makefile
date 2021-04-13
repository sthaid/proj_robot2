CC       = gcc
CPPFLAGS = -Wall -g -O2 -I../../../common/include -DTEST
LDFLAGS  = ../../../common/devices/i2c/u8g2/libu8g2.a -lstdc++ -lm

TARGETS = MPU9250_imu \
          BMP280_tp \
          STM32_adc \
          SSD1306_oled \
          BME680_tphg \
          MCP9808_temp

all: $(TARGETS)

OBJ_MPU9250_imu = MPU9250_imu.o \
                  ../../../common/devices/i2c/MPU9250_imu/mpu9250/MPU9250.o \
                  ../../../common/devices/i2c/i2c/i2c.o \
                  ../../../common/devices/i2c/i2c/I2Cdev.o \
                  ../../../common/devices/i2c/i2c/Wire.o \
                  ../../../common/util/misc.o
MPU9250_imu.o : CPPFLAGS += -I../../../common/devices/i2c/MPU9250_imu
MPU9250_imu: $(OBJ_MPU9250_imu)
	$(CC) -o $@ $(OBJ_MPU9250_imu) -lm -lstdc++

OBJ_BMP280_tp = BMP280_tp.o \
                ../../../common/devices/i2c/BMP280_tp/bmp280/BMP280.o \
                ../../../common/devices/i2c/i2c/i2c.o \
                ../../../common/devices/i2c/i2c/I2Cdev.o \
                ../../../common/devices/i2c/i2c/Wire.o \
                ../../../common/util/misc.o
BMP280_tp.o : CPPFLAGS += -I../../../common/devices/i2c/BMP280_tp
BMP280_tp: $(OBJ_BMP280_tp)
	$(CC) -o $@ $(OBJ_BMP280_tp) -lm -lstdc++

OBJ_STM32_adc = STM32_adc.o \
                ../../../common/devices/i2c/i2c/i2c.o \
                ../../../common/devices/i2c/i2c/I2Cdev.o \
                ../../../common/devices/i2c/i2c/Wire.o \
                ../../../common/util/misc.o
STM32_adc.o : CPPFLAGS += -I../../../common/devices/i2c/STM32_adc
STM32_adc: $(OBJ_STM32_adc)
	$(CC) -o $@ $(OBJ_STM32_adc) -lm -lstdc++

OBJ_SSD1306_oled = SSD1306_oled.o \
                ../../../common/devices/i2c/i2c/i2c.o \
                ../../../common/devices/i2c/i2c/I2Cdev.o \
                ../../../common/devices/i2c/i2c/Wire.o \
                ../../../common/util/misc.o
SSD1306_oled.o : CPPFLAGS += -I../../../common/devices/i2c/SSD1306_oled
SSD1306_oled: $(OBJ_SSD1306_oled)
	$(CC) -o $@ $(OBJ_SSD1306_oled) ../../../common/devices/i2c/u8g2/libu8g2.a -lm -lstdc++

OBJ_BME680_tphg = BME680_tphg.o \
                ../../../common/devices/i2c/BME680_tphg/bme680/bme680.cpp \
                ../../../common/devices/i2c/i2c/i2c.o \
                ../../../common/devices/i2c/i2c/I2Cdev.o \
                ../../../common/devices/i2c/i2c/Wire.o \
                ../../../common/util/misc.o
BME680_tphg.o : CPPFLAGS += -I../../../common/devices/i2c/BME680_tphg
BME680_tphg: $(OBJ_BME680_tphg)
	$(CC) -o $@ $(OBJ_BME680_tphg) ../../../common/devices/i2c/u8g2/libu8g2.a -lm -lstdc++

OBJ_MCP9808_temp = MCP9808_temp.o \
                ../../../common/devices/i2c/i2c/i2c.o \
                ../../../common/devices/i2c/i2c/I2Cdev.o \
                ../../../common/devices/i2c/i2c/Wire.o \
                ../../../common/util/misc.o
MCP9808_temp.o : CPPFLAGS += -I../../../common/devices/i2c/MCP9808_temp
MCP9808_temp: $(OBJ_MCP9808_temp)
	$(CC) -o $@ $(OBJ_MCP9808_temp) -lm -lstdc++

clean:
	rm -f $(TARGETS) *.o
	find ../../../common/devices/i2c/ -name \*.o -exec rm {} \;
	find ../../../common/util/ -name \*.o -exec rm {} \;
