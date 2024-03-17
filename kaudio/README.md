# Project Description

This is the code for the kaudio module. It sets up the module and all the required kernel operations and the hardware in order for the usermode side to be able to play the audio.

# Installation/Make Instructions

To make the module, you can navigate into the "kaudio" folder where the file "kaudio.c" is located and run the make command. Afterwards just transfer the compiled module file to the board with scp and run insmod command to install it.

# Usage Definition/Examples

After the module is installed, nothing should happen, but after the usermode-player is transfered to the board it can be used in order to play the audio files.

# Known Issues

The CODEC samples at a rate of approximately 45kHz and can currently only accept WAV files. Also, the removal of the module does not remove the character device within the /dev/ directory so it requires a reboot. However, it does function properly before it would be removed

# Release Notes

#### Release 0.1

The FIFO is continiously polled to see if it is full then runs a usleep_range() to allow for the samples to be sent and for the FIFO to be filled up again. It is currently not optimized as it uses a range of sleeps to cater toward different WAV files characteristics and is a random sleep length. This adds stress to the CPU as polling is something that would take cycles to check every time the polling occurs.

#### Release 0.2

Now the FIFO uses an interrupt handler to see if it is empty or full. It uses wait_event_interruptable() when the FIFO full interrupt and then sets the FIFO to sleep until the FIFO hits the empty threshold and then uses wake_up() to start the FIFO filling. This allows for better CPU usage as it stresses it less and only wakes or sleeps when the interrupt occurs. 
