CC       = gcc
CPPFLAGS = -Wall -g -O2
LDFLAGS  = 

TARGET   = gpio_test
SRC      = gpio_test.c

OBJ := $(SRC:.c=.o)
OBJ := $(OBJ:.cpp=.o)

$(TARGET): $(OBJ)
	$(CC) -o $@ $(OBJ) $(LDFLAGS)

clean:
	rm -f $(TARGET) $(OBJ)
