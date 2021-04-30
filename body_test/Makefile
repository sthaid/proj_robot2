CC       = gcc
CPPFLAGS = -Wall -g -O2 -I../body/include -I../common/include -Wno-stringop-truncation
LDFLAGS  = -lpthread -lcurses

TARGET   = bt
SRC      = bt.c 

OBJ := $(SRC:.c=.o)
OBJ := $(OBJ:.cpp=.o)

$(TARGET): $(OBJ)
	$(CC) -o $@ $(OBJ) $(LDFLAGS)

clean:
	rm -f $(TARGET) $(OBJ)
