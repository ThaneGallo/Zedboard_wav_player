# Lab 6 Kernel Mode Audio TX

This part of the assignment implements a kernel driver for audio
transmission (`kaudio`) as well as a user level application
(`usermode-player`) to play audio from a file using the kernel driver.

The lab builds on the experience from interacting with the Audio TX FIFO
from user space (Lab 5), and the echo driver developed in the
prelab. The driver copies data from the user space into a kernel
buffer and from there into the audio TX FIFO. If the TX FIFO is full the
driver sleeps and polls until space is available.

The audio setup is descripted [here](https://neu-ece-4534.github.io/audio.html).

## Overview

The instructions for ths lab are detailed in the following steps:

1.  (Reserved for feedback branch pull request. You will receive top level feedback there).
2.  [Documentation Prep](.github/STARTING_ISSUES/2.%20Documentation%20Prep.md)
3.  [AudioTX Kernel Driver and Usermod Player](.github/STARTING_ISSUES/3.%20AudioTX%20Kernel%20Driver%20and%20Usermod%20Player.md)
4.  [Error Interrupt Handler](.github/STARTING_ISSUES/4.%20Error%20Interrupt%20Handler.md)
5.  [Release 0.1 Documentation](.github/STARTING_ISSUES/5.%20Release%200.1%20Documentation.md)
6.  [Performance Impacts](.github/STARTING_ISSUES/6.%20Performance%20Impacts.md)
7.  [Release 0.2 Documentation](.github/STARTING_ISSUES/7.%20Release%200.2%20Documentation.md)

After accepting this assignment in github classroom, each step is converted into a [github issue](https://docs.github.com/en/issues). Follow the issues in numerically increasing issue number (the first issue is typically on the bottom of the list).

## General Rules

Please commit your code frequently or at e very logical break. Each commit should have a meaningful commit message and [cross reference](https://docs.github.com/en/get-started/writing-on-github/working-with-advanced-formatting/autolinked-references-and-urls#issues-and-pull-requests) the issue the commit belongs to. Ideally, there would be no commits without referencing to a github issue.

Please comment on each issue with the problems faced and your approach to solve them. Close an issue when done.
