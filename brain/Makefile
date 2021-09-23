# reference:
# - https://stackoverflow.com/questions/3220277/what-do-the-makefile-symbols-and-mean

# XXX todo
# - submake for go
# - review the -I list

SOURCES = brain.c \
          proc_cmd.c proc_cmd_hndlrs.c \
          utils/doa.c utils/leds.c utils/misc.c utils/pa.c utils/s2t.c utils/t2s.c utils/wwd.c

brain: $(SOURCES)
	gcc -g  -O2 -Wall -I. -Iutils -Idevel/repos/Porcupine/include $^ \
            -lpthread -lportaudio -lm -ldl \
            -o $@

clean:
	rm -f brain

