CC       = gcc
CPPFLAGS = -Wall -g -O2 -I../../include
LDFLAGS  = 

TARGET   = relay_test
SRC      = relay_test.c \
           ../../devices/relay.c \
           ../../util/misc.c

OBJ := $(SRC:.c=.o)
OBJ := $(OBJ:.cpp=.o)

$(TARGET): $(OBJ)
	$(CC) -o $@ $(OBJ) $(LDFLAGS)

clean:
	rm -f $(TARGET) $(OBJ)
