CC       = gcc
CPPFLAGS = -Wall -g -O2 -I../../include
LDFLAGS  = 

TARGET   = current_test
SRC      = current_test.c \
           ../../devices/current.c \
           ../../devices_i2c/STM32_adc/STM32_adc.c \
           ../../devices_i2c/i2c/i2c.c \
           ../../util/misc.c

OBJ := $(SRC:.c=.o)
OBJ := $(OBJ:.cpp=.o)

$(TARGET): $(OBJ)
	$(CC) -o $@ $(OBJ) $(LDFLAGS)

clean:
	rm -f $(TARGET) $(OBJ)
