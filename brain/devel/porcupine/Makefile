# reference:
# https://stackoverflow.com/questions/3220277/what-do-the-makefile-symbols-and-mean

TARGETS = porcupine_demo_mic tst1 tst2

build: $(TARGETS)

# unmodified porcupine demo
porcupine_demo_mic: porcupine_demo_mic.c
	gcc -g -O2 -I../repos/Porcupine/include/ $^ -ldl -lasound -o $@

run_demo:
	porcupine_demo_mic  \
            ../repos/Porcupine/lib/linux/x86_64/libpv_porcupine.so \
            ../repos/Porcupine/lib/common/porcupine_params.pv \
            ../repos/Porcupine/resources/keyword_files/linux/porcupine_linux.ppn \
            0.5 \
            sysdefault:CARD=Device

tst1: tst1.c ../portaudio/pa_utils.c
	gcc -g -O2 \
            -I../repos/Porcupine/include/ -I../portaudio $^ \
            -ldl -lportaudio -lpthread -o $@

tst2: tst2.c ../portaudio/pa_utils.c
	gcc -g  -O2 -Wno-unused-result \
            -I../repos/Porcupine/include/ -I../portaudio $^ \
            -ldl -lportaudio -lpthread -o $@

clean:
	rm -f $(TARGETS)

