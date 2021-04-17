CC       = gcc
CPPFLAGS = -Wall -g -O2 -I../../../common/include -I../../../body/include
LDFLAGS  = -lpthread -lstdc++ -lm

TARGET   = env_test
SRC      = env_test.c \
           ../../../common/devices/env.c \
           ../../../common/devices/i2c/BMP280_tp/BMP280_tp.cpp \
           ../../../common/devices/i2c/BMP280_tp/bmp280/BMP280.cpp \
           ../../../common/devices/i2c/i2c/Wire.c \
           ../../../common/devices/i2c/i2c/i2c.c \
           ../../../common//util/misc.c

OBJ := $(SRC:.c=.o)
OBJ := $(OBJ:.cpp=.o)

$(TARGET): $(OBJ)
	$(CC) -o $@ $(OBJ) $(LDFLAGS)

clean:
	rm -f $(TARGET) $(OBJ)
