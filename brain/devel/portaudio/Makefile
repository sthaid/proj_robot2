# reference:
# https://stackoverflow.com/questions/3220277/what-do-the-makefile-symbols-and-mean

TARGETS = octave devs record plot

build: $(TARGETS)

octave: octave.c pa_utils.c
	gcc -g -Wall -O2 -I. -lportaudio -lm -o $@ $^ 

devs: devs.c pa_utils.c
	gcc -g -Wall -O2 -I. -lportaudio -lm -o $@ $^ 

record: record.c pa_utils.c sf_utils.c
	gcc -g -Wall -O2 -I. -lportaudio -lsndfile -lm -o $@ $^ 

plot: plot.c util/util_sdl.c util/util_misc.c util/util_jpeg.c util/util_png.c
	gcc -g -Wall -O2 \
            -I. -Iutil -I/usr/include/SDL2 \
            -lSDL2 -lSDL2_ttf -lm -ljpeg -lpng \
            -o $@ $^ 

clean:
	rm -f $(TARGETS) *.wav

