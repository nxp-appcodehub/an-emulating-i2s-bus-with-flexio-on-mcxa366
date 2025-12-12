# dev_audio_generator




## Overview

The example is used to test the function of emulating I2S by FlexIO.
FlexIO is used as I2S master, it can receive the audio from PC via AUX_IN interface and then playback it via HEADSET interface on PmodI2S2 board.



## System Requirement

### Hardware requirements

- Type C USB cable
- 3.5mm audio cable
- FRDM-MCXA276
- PmodI2S2
- Personal Computer(PC)
- 3.5mm headphone


### Software requirements

- MCUXpresso IDE v24.12 [Build 148] [2025-01-10]



## Getting Started

### Hardware Settings

Connect the PmodI2S2 board to FRDM-MCXA276 PMOD header J7, then connect the FlexIO I2S TX BCLK/MCLK/FS to RX BCLK/MCLK/FS.
- I2S TX BCLK - I2S RX BCLK : J6_5/P1_2/FLEXIO_D10 - J8_3/P3_27/FLEXIO_D27
- I2S TX MCLK - I2S RX MCLK : J2_11/P1_6/FLEXIO_D14 - J2_7/P1_4/FLEXIO_D12
- I2S TX FS   - I2S RX FS   : J4_10/P1_0/FLEXIO_D8 - J2_19/P1_5/FLEXIO_D13


### Prepare the example

1.  Connect a USB cable between the PC host and the debugger USB port on the board to provide power supply (the example is self-powered).
2.  Download the program to the target board via IDE.
3.  Either press the reset button on your board or launch the debugger in your IDE to begin running the demo.
4.  Use a 3.5mm audio cable to connect PC and the AUX_IN interface of PmodI2S2 board.
5.  Coonect a 3.5mm headphone to HEADSET interface of PmodI2S2 board.
6. Play any audio file on PC, then you can hear the audio from 3.5mm headphone.



