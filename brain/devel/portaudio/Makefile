
TARGETS = octave 

build: $(TARGETS)

octave: octave.c
	gcc -Wall -O2 -o octave octave.c -lportaudio -lm

clean:
	rm $(TARGETS)
