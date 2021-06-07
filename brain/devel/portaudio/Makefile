
TARGETS = octave devs record

build: $(TARGETS)

octave: octave.c pa_utils.c pa_utils.h
	gcc -Wall -O2 -o octave octave.c pa_utils.c -lportaudio -lm

devs: devs.c pa_utils.c pa_utils.h
	gcc -Wall -O2 -o devs devs.c pa_utils.c -lportaudio -lm

record: record.c pa_utils.c pa_utils.h sf_utils.c sf_utils.h
	gcc -Wall -O2 -o record record.c pa_utils.c sf_utils.c -lportaudio -lsndfile -lm

clean:
	rm $(TARGETS) *.wav
