# reference:
# https://stackoverflow.com/questions/3220277/what-do-the-makefile-symbols-and-mean

TARGETS = gen devs record plot filters analyze

build: $(TARGETS)

gen: gen.c pa_utils.c sf_utils.c
	gcc -g -Wall -O2 -I. $^ -lportaudio -lsndfile -lm -o $@ 

devs: devs.c pa_utils.c
	gcc -g -Wall -O2 -I. $^ -lportaudio -lm -o $@ 

record: record.c pa_utils.c sf_utils.c 
	gcc -g -Wall -O2 -I. $^ -lportaudio -lsndfile -lm -o $@ 

plot: plot.c pa_utils.c sf_utils.c util/util_sdl.c util/util_misc.c util/util_jpeg.c util/util_png.c
	gcc -g -Wall -O2 -I. -Iutil -I/usr/include/SDL2 $^ \
            -lSDL2 -lSDL2_ttf -ljpeg -lpng -lportaudio -lsndfile -lm -lpthread \
            -o $@ 

filters: filters.c pa_utils.c sf_utils.c util/util_sdl.c util/util_misc.c util/util_jpeg.c util/util_png.c
	gcc -g -Wall -O2 -I. -Iutil -I/usr/include/SDL2 $^ \
            -lSDL2 -lSDL2_ttf -ljpeg -lpng -lfftw3 -lsndfile -lportaudio -lpthread -lm \
            -o $@ 

analyze: analyze.c sf_utils.c poly_fit.c
	gcc -g -Wall -O2 -I. -Iutil $^ -lsndfile -lpthread -lm -o $@ 

clean:
	rm -f $(TARGETS)

