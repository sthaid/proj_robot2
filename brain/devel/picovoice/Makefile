picovoice_demo_mic:
	cc -std=c99 -O3 -o picovoice_demo_mic -I ./include picovoice_demo_mic.c -ldl -lasound

clean:
	rm picovoice_demo_mic

DIST         = /usr/local/lib/python3.7/dist-packages
AUDIO_DEVICE = "plughw:CARD=seeed4micvoicec,DEV=0"

run:
	./picovoice_demo_mic \
            ./lib/libpicovoice.so \
            $(DIST)/pvporcupine/lib/common/porcupine_params.pv \
            $(DIST)/pvporcupine/resources/keyword_files/raspberry-pi/picovoice_raspberry-pi.ppn \
            0.5 \
            $(DIST)/pvrhino/lib/common/rhino_params.pv \
            $(DIST)/pvrespeakerdemo/respeaker_raspberry-pi.rhn \
            0.5 \
            $(AUDIO_DEVICE)

