SUBDIRS = picovoice \
          porcupine \
          portaudio \
          speech_to_text \
          text_to_speech

ARCH := $(shell arch)

ifeq ($(ARCH),armv7l)
SUBDIRS += leds
endif
  
.PHONY: build clean

build:
	for d in $(SUBDIRS) ; do echo; make -C $$d || exit 1; done

clean:
	for d in $(SUBDIRS) ; do echo; make -C $$d clean || exit 1; done

