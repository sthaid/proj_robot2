CC       = gcc
CPPFLAGS = -Wall -g -O2 -I../../include
LDFLAGS  = -lpthread

TARGET   = enc_test
SRC      = enc_test.c \
           ../../devices/encoder.c \
           ../../util/misc.c

OBJ := $(SRC:.c=.o)
OBJ := $(OBJ:.cpp=.o)

$(TARGET): $(OBJ)
	$(CC) -o $@ $(OBJ) $(LDFLAGS)

clean:
	rm -f $(TARGET) $(OBJ)
