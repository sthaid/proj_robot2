CC       = gcc
CPPFLAGS = -Wall -g -O2 -I../../include
LDFLAGS  = -lpthread

TARGET   = mc_test
SRC      = mc_test.c \
           ../../devices/mc.c \
           ../../util/misc.c

OBJ := $(SRC:.c=.o)
OBJ := $(OBJ:.cpp=.o)

$(TARGET): $(OBJ)
	$(CC) -o $@ $(OBJ) $(LDFLAGS)

clean:
	rm -f $(TARGET) $(OBJ)
