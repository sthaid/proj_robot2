TARGETS = synthesize_file

.PHONY: build clean clobber test

ARCH := $(shell arch)
ifeq ($(ARCH),armv7l)
OUTDEV = sysdefault:CARD=Device
else
OUTDEV = default
endif

build: $(TARGETS)

synthesize_file: synthesize_file.go go.mod
	go build synthesize_file.go

go.mod:
	go mod init `basename ${PWD}`
	go get cloud.google.com/go/speech/apiv1

clean:
	rm -f $(TARGETS)

clobber: clean
	rm -f go.mod go.sum

test:
	echo "Open the pod bay door. What is your favorite color" > hello.txt
	./synthesize_file --text hello.txt
	/usr/bin/aplay  -D $(OUTDEV) -t raw -r 24000 -c 1 -f S16_LE output.raw
	rm hello.txt output.raw
