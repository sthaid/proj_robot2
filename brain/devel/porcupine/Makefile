# reference:
# https://stackoverflow.com/questions/3220277/what-do-the-makefile-symbols-and-mean

TARGETS = porcupine_demo_mic tst1 tst2

ARCH := $(shell arch)
	
ifeq ($(ARCH),armv7l)
LIBRARY = ../repos/picovoice/sdk/c/lib/raspberry-pi/cortex-a72/libpicovoice.so
MODEL   = ../repos/Porcupine/lib/common/porcupine_params.pv
KEYWORD = ../repos/Porcupine/resources/keyword_files/raspberry-pi/porcupine_raspberry-pi.ppn
INDEV   = sysdefault:CARD=seeed4micvoicec
CDEFS   = -DRASPBERRY_PI
else
LIBRARY = ../repos/Porcupine/lib/linux/x86_64/libpv_porcupine.so
MODEL   = ../repos/Porcupine/lib/common/porcupine_params.pv
KEYWORD = ../repos/Porcupine/resources/keyword_files/linux/porcupine_linux.ppn
INDEV   = sysdefault:CARD=Device
CDEFS   = -DLINUX
endif

build: $(TARGETS)

porcupine_demo_mic: porcupine_demo_mic.c
	gcc -g -O2 -I../repos/Porcupine/include/ $^ -ldl -lasound -o $@

run_demo:
	porcupine_demo_mic  \
            $(LIBRARY) \
            $(MODEL) \
            $(KEYWORD) \
            0.5 \
            $(INDEV)

tst1: tst1.c ../portaudio/pa_utils.c
	gcc -g -O2 $(CDEFS) \
            -I../repos/Porcupine/include/ -I../portaudio $^ \
            -ldl -lportaudio -lpthread -o $@

tst2: tst2.c ../portaudio/pa_utils.c
	gcc -g  -O2 -Wno-unused-result $(CDEFS) \
            -I../repos/Porcupine/include/ -I../portaudio $^ \
            -ldl -lportaudio -lpthread -o $@

clean:
	rm -f $(TARGETS)

