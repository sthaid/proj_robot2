# Voice Controlled Robot

The Robot is made up of two subsystems, the body and brain.

## Body

The body consists of 
* Raspberry Pi with SeeedStudio Grove HAT
* Motors, Motor Controllers, and Quadrature Encoders
* IMU Sensor (Accelerometer, Magnetometer, Gyroscope)
* Temperature and Pressure Sensors
* Proximity Sensors
* Current Sensor
* OLED Display

The body receives and processes commands from the brain.

## Brain

The brain is mounted on a platform above the center of the body.

The brain consists of:
* Raspberry Pi with SeeedStudio Respeaker HAT
* USB Speaker

High level software flow:
```
  do forever
    Wait for 'Porcupine' wake word.
    Display the direction of sound arrival on LEDs.
    Send the following sound data to Google to be converted to speech.
    Match the speech with defined grammar, and obtain the software handler name.
    Call the handler to process the command.
    The handler will process the command and play audio response. Google text to speech
     is used to create the audio response wav file.
  enddo
```

## Grammar Examples

* go forward 3 feet
* turn around
* turn halfway around
* turn 90 degrees counter-clockwise
* turn to face north
* turn to face compass heading 270 
* turn to face me
* run test number 2
* calibrate compass
* set volume to 30 percent
* set brightness to 50 percent
* status report
* what is your compass heading
* what is your voltage
* turn lights on
* search wikipedia for 'search string'
* play 'song title'
* what is my name
* what time is it
* weather report
* how do you feel

## Software

The code is mostly written in C, except when sample code or other constraints required other languages such as Go, C++, and Python.

The following software packages or sample code are used:

* SeeedStudio samples for Grove devices and 4-Mic-Respeaker: https://www.seeedstudio.com/
* Google Text-To-Speech: https://cloud.google.com/text-to-speech
* Google Speech-To-Text: https://cloud.google.com/speech-to-text/
* Picovoice/Porcupine Wake Word Engine: https://github.com/Picovoice/Porcupine
* Google Custom-Search: https://developers.google.com/custom-search/
* Beautiful Soup is a Python library for pulling data out of HTML: https://beautiful-soup-4.readthedocs.io/en/latest/   https://stackoverflow.com/questions/53804643/how-can-i-get-a-wikipedia-articles-text-using-python-3-with-beautiful-soup/53804726
* Pololu Simple Motor Controller Examples: https://www.pololu.com/docs/0J77
* How to use a quadrature encoder: https://cdn.sparkfun.com/datasheets/Robotics/How%20to%20use%20a%20quadrature%20encoder.pdf
* OLED Display: https://github.com/olikraus/u8g2
* Audio I/O Library: http://www.portaudio.com
* Library for reading and writing sound files: http://www.mega-nerd.com/libsndfile/
* C subroutine library for computing the discrete Fourier transform: http://www.fftw.org/
* Respeaker LEDs: https://github.com/tinue/apa102-pi
* TP-Link WiFi SmartPlug Client: https://github.com/softScheck/tplink-smartplug
* Polynomial Fitting: https://www.bragitoff.com/2018/06/polynomial-fitting-c-program/
