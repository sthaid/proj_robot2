# reference:
# https://stackoverflow.com/questions/3220277/what-do-the-makefile-symbols-and-mean

TARGETS = gen devs record plot filters analyze

ARCH := $(shell arch)

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

ifeq ($(ARCH),armv7l)
analyze: analyze.c sf_utils.c pa_utils.c poly_fit.c apa102.c 
	gcc -g -Wall -O2 -I. -DRASPBERRY_PI $^ -lsndfile -lportaudio -lwiringPi -lpthread -lm -o $@ 
else
analyze: analyze.c sf_utils.c pa_utils.c poly_fit.c
	gcc -g -Wall -Wno-unused-function -O2 -I. -DLINUX $^ -lsndfile -lportaudio -lpthread -lm -o $@ 
endif

clean:
	rm -f $(TARGETS)

