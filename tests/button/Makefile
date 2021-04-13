CC       = gcc
CPPFLAGS = -Wall -g -O2 -I../../include
LDFLAGS  = -lpthread

TARGET   = button_test
SRC      = button_test.c \
           ../../devices/button.c \
           ../../util/misc.c

XXX= ../../devices/button.c \

OBJ := $(SRC:.c=.o)
OBJ := $(OBJ:.cpp=.o)

$(TARGET): $(OBJ)
	$(CC) -o $@ $(OBJ) $(LDFLAGS)

clean:
	rm -f $(TARGET) $(OBJ)
