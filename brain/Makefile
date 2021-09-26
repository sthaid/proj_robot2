CC       = gcc
CFLAGS   = -g  -O2 -Wall -I. -Iutils -Idevel/repos/Porcupine/include
LDFLAGS  =  -lpthread -lportaudio -lm -ldl

TARGET   = brain
SOURCES  = brain.c \
           proc_cmd.c proc_cmd_hndlrs.c \
           utils/doa.c utils/leds.c utils/misc.c utils/pa.c utils/s2t.c utils/t2s.c utils/wwd.c

OBJ := $(SOURCES:.c=.o)

$(TARGET): $(OBJ)
	$(CC) -o $@ $(OBJ) $(LDFLAGS)

clean:
	rm -f $(TARGET) $(OBJ)

