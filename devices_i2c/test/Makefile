TARGET = devtest
CFLAGS = -Wall -g -O2

SRC = devtest.c \
     ../MCP9808_temp/MCP9808_temp.c \
     ../STM32_adc/STM32_adc.c \
     ../BME680_tphg/BME680_tphg.c ../BME680_tphg/bme680/bme680.c \
     ../SSD1306_oled/SSD1306_oled.c \
     ../i2c/i2c.c \
      ../../util/misc.c

$(TARGET): $(SRC:.c=.o)
	gcc -o $@ $(SRC:.c=.o) ../u8g2/libu8g2.a

clean:
	rm -f $(TARGET) $(SRC:.c=.o)
