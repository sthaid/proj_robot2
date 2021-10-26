# reference:
# https://stackoverflow.com/questions/3220277/what-do-the-makefile-symbols-and-mean

TARGETS = mic livecaption

build: $(TARGETS)

mic: mic.c ../portaudio/pa_utils.c
	gcc -g -Wall -O2 -I. -I../portaudio $^ -lportaudio -o $@ 

livecaption: livecaption.go go.mod
	go build livecaption.go

go.mod:
	go mod init `basename ${PWD}`
	go get cloud.google.com/go/speech/apiv1

clean:
	rm -f $(TARGETS)

clobber: clean
	rm -f go.mod go.sum

