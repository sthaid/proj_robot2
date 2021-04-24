CC       = gcc
CPPFLAGS = -Wall -g -O2 -I../common/include -Iinclude
LDFLAGS  = ../common/devices/i2c/u8g2/libu8g2.a -lpthread -lm -lcurses -lstdc++

TARGET   = body
SRC      = main.c \
           drive.c \
           oled_ctlr.c \
           ../common/devices/mc.c \
           ../common/devices/encoder.c \
           ../common/devices/proximity.c \
           ../common/devices/button.c \
           ../common/devices/current.c \
           ../common/devices/oled.c \
           ../common/devices/env.c \
           ../common/devices/imu.c \
           ../common/devices/i2c/STM32_adc/STM32_adc.c \
           ../common/devices/i2c/SSD1306_oled/SSD1306_oled.c \
           ../common/devices/i2c/BMP280_tp/BMP280_tp.cpp \
           ../common/devices/i2c/BMP280_tp/bmp280/BMP280.cpp \
           ../common/devices/i2c/MPU9250_imu/MPU9250_imu.cpp \
           ../common/devices/i2c/MPU9250_imu/mpu9250/MPU9250.cpp \
           ../common/devices/i2c/i2c/Wire.c \
           ../common/devices/i2c/i2c/I2Cdev.cpp \
           ../common/devices/i2c/i2c/i2c.c \
           ../common//util/misc.c

OBJ := $(SRC:.c=.o)
OBJ := $(OBJ:.cpp=.o)

$(TARGET): $(OBJ)
	$(CC) -o $@ $(OBJ) $(LDFLAGS)

clean:
	rm -f $(TARGET) $(OBJ)
