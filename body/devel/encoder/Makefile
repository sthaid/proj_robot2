CC       = gcc
CPPFLAGS = -Wall -g -O2 -I../../../common/include -I../../../body/include
LDFLAGS  = -lpthread

TARGET   = enc_test
SRC      = enc_test.c \
           ../../../common/devices/encoder.c \
           ../../../common//util/misc.c

OBJ := $(SRC:.c=.o)
OBJ := $(OBJ:.cpp=.o)

$(TARGET): $(OBJ)
	$(CC) -o $@ $(OBJ) $(LDFLAGS)

clean:
	rm -f $(TARGET) $(OBJ)
