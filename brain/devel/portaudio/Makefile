
TARGETS = octave devs

build: $(TARGETS)

octave: octave.c pa_utils.c pa_utils.h
	gcc -Wall -O2 -o octave octave.c pa_utils.c -lportaudio -lm

devs: devs.c pa_utils.c pa_utils.h
	gcc -Wall -O2 -o devs devs.c pa_utils.c -lportaudio -lm

clean:
	rm $(TARGETS)
