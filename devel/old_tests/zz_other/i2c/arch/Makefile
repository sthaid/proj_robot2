CC       = gcc
CPPFLAGS = -Wall -g -O2 -I../../include
LDFLAGS  = ../../devices_i2c/u8g2/libu8g2.a -lstdc++ -lm

TARGET   = mag_test
SRC      = mag_test.c \
           ../../devices_i2c/MCP9808_temp/MCP9808_temp.c \
           ../../devices_i2c/STM32_adc/STM32_adc.c \
           ../../devices_i2c/SSD1306_oled/SSD1306_oled.c \
           ../../devices_i2c/BME680_tphg/BME680_tphg.c \
           ../../devices_i2c/BME680_tphg/bme680/bme680.cpp \
           ../../devices_i2c/MPU9250_imu/MPU9250_imu.cpp \
           ../../devices_i2c/MPU9250_imu/mpu9250/MPU9250.cpp \
           ../../devices_i2c/BMP280_tp/BMP280_tp.c \
           ../../devices_i2c/BMP280_tp/bmp280/BMP280.cpp \
           ../../devices_i2c/i2c/i2c.c \
           ../../devices_i2c/i2c/I2Cdev.cpp \
           ../../devices_i2c/i2c/Wire.cpp \
           ../../util/misc.c

OBJ := $(SRC:.c=.o)
OBJ := $(OBJ:.cpp=.o)

$(TARGET): $(OBJ)
	$(CC) -o $@ $(OBJ) $(LDFLAGS)

clean:
	rm -f $(TARGET) $(OBJ)
