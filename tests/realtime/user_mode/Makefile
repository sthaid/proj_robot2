CC       = gcc
CPPFLAGS = -Wall -g -O2 -I../../../include
LDFLAGS  = -lpthread

TARGET   = rt_gpio_test
SRC      = rt_gpio_test.c \
           ../../../util/misc.c 

OBJ := $(SRC:.c=.o)
OBJ := $(OBJ:.cpp=.o)

$(TARGET): $(OBJ)
	$(CC) -o $@ $(OBJ) $(LDFLAGS)

clean:
	rm -f $(TARGET) $(OBJ)
