ESP32-NESEMU, a Nintendo Entertainment System emulator for the ESP32
====================================================================

This is a quick and dirty port of Nofrendo, a Nintendo Entertainment System emulator. It lacks sound, but can emulate a NES at close
to full speed, albeit with some framedrop due to the way the display is driven.

Warning
-------

This is a proof-of-concept of a proof-of-concept for hackerboxes #20 badge and not an official application note. As such, this code is entirely unsupported by Espressif.


Compiling
---------

This code is an esp-idf project. You will need esp-idf to compile it. Newer versions of esp-idf may introduce incompatibilities with this code;
for your reference, the code was tested against commit 37765d0071dce62cf6ffb425e0aab3fcab8c0897 of esp-idf.

This version also uses the following TFT library for a custom rom loading boot screen: https://github.com/loboris/ESP32_TFT_library
Install this in a directory right outside of this project directory.
It requires these two directories to build. Create soft links.

cd components
ln -s ../../ESP32_TFT_library-master/components/tft
ln -s ../../ESP32_TFT_library-master/components/spidriver/

Then replace tftspi.h with tftspi_hb20.h (renaming it to tftspi.h) in the components/tft directory.

The rom names can be updated in main/main.c at line 54.

Display
-------

Using "make menuconfig" select the "Nofrendo ESP32-specific configuration" and change the hardware to ESP32 Hackerbox #20.
This setting will correct the pins below and rotate the screen properly.

Below is the original documentation for the display:
To display the NES output, please connect a 320x240 ili9341-based SPI display to the ESP32 in this way:

    =====  =======================
    Pin    GPIO
    =====  =======================
    MISO   25
    MOSI   23
    CLK    19
    CS     22
    DC     21
    RST    18
    BCKL   5
    =====  =======================

(BCKL = backlight enable)

Also connect the power supply and ground. For now, the LCD is controlled using a SPI peripheral, fed using the 2nd CPU. This is less than ideal; feeding
the SPI controller using DMA is better, but was left out due to this being a proof of concept.


Controller
----------

When you enable the Hackerbox hardware, the touchpads will be enabled in the "make menuconfig".

There is an additional option to switch the select combo buttons.
Start is non-boot button on the esp32.
Select is the left/right (or up/down from the menuconfig) pressed simultaneously.

flashrom.sh need one addition arg for the slot location of the rom.
flashrom.sh <name> <0-5>

Original docs:
To control the NES, connect a Playstation 1 or 2 controller as such:

    =====  =====
    Pin    GPIO
    =====  =====
    CLK    14
    DAT    27
    ATT    16
    CMD    2
    =====  =====

Also connect the power and ground lines. Most PS1/PS2 controllers work fine from a 3.3V power supply, if a 5V one is unavailable.

ROM
---
This NES emulator does not come with a ROM. Please supply your own and flash to address 0x00100000. You can use the flashrom.sh script as a template for doing so.

Copyright
---------

Code in this repository is Copyright (C) 2016 Espressif Systems, licensed under the Apache License 2.0 as described in the file LICENSE. Code in the
components/nofrendo is Copyright (c) 1998-2000 Matthew Conte (matt@conte.com) and licensed under the GPLv2.
