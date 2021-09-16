# reference:
# - https://stackoverflow.com/questions/3220277/what-do-the-makefile-symbols-and-mean

# XXX todo
# - submake for go
# - review the -I list

brain: brain.c utils/leds.c utils/misc.c utils/pa.c utils/t2s.c utils/wwd.c
	gcc -g  -O2 -I. -Iutils -Idevel/repos/Porcupine/include $^ \
            -lpthread -lportaudio -lm -ldl \
            -o $@

clean:
	rm -f brain

