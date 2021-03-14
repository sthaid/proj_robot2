TARGET = devtest
CPPFLAGS = -Wall -g -O2

SRC = devtest.c \
     ../MCP9808_temp/MCP9808_temp.c \
     ../STM32_adc/STM32_adc.c \
     ../SSD1306_oled/SSD1306_oled.c \
     ../BME680_tphg/BME680_tphg.c ../BME680_tphg/bme680/bme680.cpp \
     ../MPU9250_imu/MPU9250_imu.cpp ../MPU9250_imu/mpu9250/MPU9250.cpp \
     ../i2c/i2c.c ../i2c/I2Cdev.cpp \
      ../../util/misc.c

OBJ := $(SRC:.c=.o)
OBJ := $(OBJ:.cpp=.o)

$(TARGET): $(OBJ)
	gcc -o $@ $(OBJ) ../u8g2/libu8g2.a -lstdc++ -lm

clean:
	rm -f $(TARGET) $(OBJ)
