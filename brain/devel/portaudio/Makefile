# reference:
# https://stackoverflow.com/questions/3220277/what-do-the-makefile-symbols-and-mean

TARGETS = gen devs record plot analyze filters

build: $(TARGETS)

gen: gen.c pa_utils.c
	gcc -g -Wall -O2 -I. $^ -lportaudio -lm -o $@ 

devs: devs.c pa_utils.c
	gcc -g -Wall -O2 -I. $^ -lportaudio -lm -o $@ 

record: record.c pa_utils.c sf_utils.c file_utils.c
	gcc -g -Wall -O2 -I. $^ -lportaudio -lsndfile -lm -o $@ 

plot: plot.c pa_utils.c file_utils.c util/util_sdl.c util/util_misc.c util/util_jpeg.c util/util_png.c
	gcc -g -Wall -O2 -I. -Iutil -I/usr/include/SDL2 $^ \
            -lSDL2 -lSDL2_ttf -ljpeg -lpng -lportaudio -lm -lpthread \
            -o $@ 

analyze: analyze.c file_utils.c ../../../common/util/misc.c
	gcc -g -Wall -O2 -I. -I../../../common/include $^ -lm -o $@ 

filters: filters.c util/util_sdl.c util/util_misc.c util/util_jpeg.c util/util_png.c
	gcc -g -Wall -O2 -I. -Iutil -I/usr/include/SDL2 $^ \
            -lSDL2 -lSDL2_ttf -ljpeg -lpng -lfftw3 -lm \
            -o $@ 

clean:
	rm -f $(TARGETS) *.wav

