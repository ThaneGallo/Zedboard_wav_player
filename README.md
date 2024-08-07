# Lab Instructions

[Lab Instructions](LabInstructions.md)

# Project Description

This lab's objective is to create an audio module in order to stream audio from the board. It includes creating the audio module as well as modifying the user side code from lab5 in order to integrate it with the audio module. In order to use the code you need to set up both the kaudio module and the usermode-player instructions for which are linked below.

# Kaudio Module

[kaudio description](kaudio/README.md)

# Usermode-player

[usermode-player description](usermode-player/README.md)

# Design Discussion

#### 6.1:

Write is an atomic operation that performs the action of writing to a file pointer object, it is a call to the OS made by the aplication and is non-buffering which would mean that the entire write is performed at once. fwrite is buffering so it performs the write in several chunks in a buffered stream this allows for more data to be processed faster. This would mean that if we used open and write instead of the buffered variants the data would be distorted more.

#### 6.2:

Every poll to the FIFO is a small slow down and decease in system performance in the ideal world the polling would only occur when the full threshold is reached rather than at multiple intervals. The initial range of the usleep was from 1000 to 2000 which is from .001 sec to .002 sec. Although this is a small timing the sound seems good and plays correctly as the usleep increases the system load decreases to a point without distorting the audio and in reverse if the timing is smaller the system load increases as it polls more frequently and sleeps less. If the usleep was commented out completely we can see that the system load is much higher as it polls as fast as the system is able to greatly increasing the resources required

#### 6.3:

The AXI FIFO can use the wakeup interruptable to check when the fifo hits its programmable empty and the filling would need to begin again and because it is caused by an interrupt it only activates when it absolutely needs to rather than at a regular time interval. wake_up and wait_event_interruptable work in tandem and work differently than the polling because one makes the process sleep and wait until the interrupt occurs while the other only wakes it up when it reaches a threshold so it only triggers at optimal moments rather than the timed polling that the pervious function used.
