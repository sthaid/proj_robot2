CC       = gcc
CPPFLAGS = -Wall -g -O2 -I../../../common/include -I../../../body/include
LDFLAGS  = -lpthread ../../../common/devices/i2c/u8g2/libu8g2.a

TARGET   = oled_test
SRC      = oled_test.c \
           ../../../common/devices/oled.c \
           ../../../common/devices/i2c/SSD1306_oled/SSD1306_oled.c \
           ../../../common/devices/i2c/i2c/i2c.c \
           ../../../common//util/misc.c

OBJ := $(SRC:.c=.o)
OBJ := $(OBJ:.cpp=.o)

$(TARGET): $(OBJ)
	$(CC) -o $@ $(OBJ) $(LDFLAGS)

clean:
	rm -f $(TARGET) $(OBJ)
