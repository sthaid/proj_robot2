CC       = gcc
CPPFLAGS = -Wall -g -O2 -I../../../common/include -I../../../body/include
LDFLAGS  = -lpthread -lstdc++ -lm

TARGET   = imu_test
SRC      = imu_test.c \
           ../../../common/devices/imu.c \
           ../../../common/devices/i2c/MPU9250_imu/MPU9250_imu.cpp \
           ../../../common/devices/i2c/MPU9250_imu/mpu9250/MPU9250.cpp \
           ../../../common/devices/i2c/i2c/I2Cdev.cpp \
           ../../../common/devices/i2c/i2c/i2c.c \
           ../../../common//util/misc.c

OBJ := $(SRC:.c=.o)
OBJ := $(OBJ:.cpp=.o)

$(TARGET): $(OBJ)
	$(CC) -o $@ $(OBJ) $(LDFLAGS)

clean:
	rm -f $(TARGET) $(OBJ)
