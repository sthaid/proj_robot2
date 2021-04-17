CC       = gcc
CPPFLAGS = -Wall -g -O2 -I../../../common/include -I../../../body/include
LDFLAGS  = -lpthread

TARGET   = current_test
SRC      = current_test.c \
           ../../../common/devices/current.c \
           ../../../common/devices/i2c/STM32_adc/STM32_adc.c \
           ../../../common/devices/i2c/i2c/i2c.c \
           ../../../common//util/misc.c

OBJ := $(SRC:.c=.o)
OBJ := $(OBJ:.cpp=.o)

$(TARGET): $(OBJ)
	$(CC) -o $@ $(OBJ) $(LDFLAGS)

clean:
	rm -f $(TARGET) $(OBJ)
