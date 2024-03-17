# Project Description

This is the user side code developed in Lab5 with slight modification in order to use the audio module to play samples instead of writing directly into the fifo.

# Installation/Make Instructions

To get the code running on the board from the usermode-hw-player.c file, compile it on your local machine using the "make" command within the directory where these files are located. This action generates an executable file, which can then be transferred to the Zynq board using the "scp" command.

# Usage Definition/Examples

Upon transferring, connect to the board via SSH, navigate to the directory containing the executables, and run them directly to initiate the audio playback functionality. You also need to give the file name of the wave file to run on, the file available on the board is "filename". You can play it by running the following command: ./sndsample_u "filename". In order to successfully use the usermode-player you must first install the kaudio module.

# Release Notes

#### Release 0.1

This version is a user-mode application that interfaces with the kaudio module to play audio. The application writes to the character device and configures the I2S controller and CODEC for audio playback. This setup allows for real-time streaming of audio samples to the board's audio output, adhering to the constraints necessary for clear and undistorted sound.
