picovoice_demo_mic:
	cc -std=c99 -O3 -o picovoice_demo_mic -I../repos/picovoice/sdk/c/include picovoice_demo_mic.c -ldl -lasound

clean:
	rm picovoice_demo_mic

# some of the files referenced below were copied from:
#  /usr/local/lib/python3.7/dist-packages/pvporcupine/lib/common/porcupine_params.pv
#  /usr/local/lib/python3.7/dist-packages/pvporcupine/resources/keyword_files/raspberry-pi/picovoice_raspberry-pi.ppn
#  /usr/local/lib/python3.7/dist-packages/pvrhino/lib/common/rhino_params.pv
#  /usr/local/lib/python3.7/dist-packages/pvrespeakerdemo/respeaker_raspberry-pi.rhn
#
# Other wake word files are available in 
#   /usr/local/lib/python3.7/dist-packages/pvporcupine/resources/keyword_files/raspberry-pi

library_path          = ../repos/picovoice/sdk/c/lib/raspberry-pi/cortex-a72/libpicovoice.so
porcupine_model_path  = porcupine/porcupine_params.pv
keyword_path          = porcupine/picovoice_raspberry-pi.ppn
porcupine_sensitivity = 0.5
rhino_model_path      = rhino/rhino_params.pv
rhino_sensitivity     = 0.5
input_audio_device    = "plughw:CARD=seeed4micvoicec,DEV=0"

run:
	./picovoice_demo_mic \
            $(library_path) \
            $(porcupine_model_path) \
            $(keyword_path) \
            $(porcupine_sensitivity) \
            $(rhino_model_path) \
            rhino/respeaker_raspberry-pi.rhn \
            $(rhino_sensitivity) \
            $(input_audio_device)

run_cm:
	./picovoice_demo_mic \
            $(library_path) \
            $(porcupine_model_path) \
            $(keyword_path) \
            $(porcupine_sensitivity) \
            $(rhino_model_path) \
            rhino/cm_en_raspberry-pi_2021-06-30-utc_v1_6_0.rhn \
            $(rhino_sensitivity) \
            $(input_audio_device)

run_cm2:
	./picovoice_demo_mic \
            $(library_path) \
            $(porcupine_model_path) \
            $(keyword_path) \
            $(porcupine_sensitivity) \
            $(rhino_model_path) \
            rhino/cm2_en_raspberry-pi_2021-06-30-utc_v1_6_0.rhn \
            $(rhino_sensitivity) \
            $(input_audio_device)
