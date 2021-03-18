CC       = gcc
CPPFLAGS = -Wall -g -O2 -I../../include
LDFLAGS  = -lpthread

TARGET   = proximity_test
SRC      = proximity_test.c \
           ../../devices/proximity.c \
           ../../util/misc.c

OBJ := $(SRC:.c=.o)
OBJ := $(OBJ:.cpp=.o)

$(TARGET): $(OBJ)
	$(CC) -o $@ $(OBJ) $(LDFLAGS)

clean:
	rm -f $(TARGET) $(OBJ)
