CC       = gcc
CPPFLAGS = -Wall -g -O2 -I../body/include -I../common/include
LDFLAGS  = -lpthread

TARGET   = bt
SRC      = bt.c \
           ../common/util/misc.c

OBJ := $(SRC:.c=.o)
OBJ := $(OBJ:.cpp=.o)

$(TARGET): $(OBJ)
	$(CC) -o $@ $(OBJ) $(LDFLAGS)

clean:
	rm -f $(TARGET) $(OBJ)
