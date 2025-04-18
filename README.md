### ESP32/M5StackPlus based Atari 2600 classic joystick firmware

## Description
This project allows you to convert an existing Atari 2600 classic joystick (original or clone) into a wireless Bluetooth controller, compatible with platforms such as RetroPie, Windows (Stella), and more.

With minimal wiring to the joystick's internal PCB, you can connect it to an ESP32-based board like the M5StackPlus1.1, which conveniently provides five exposed GPIO pins (3 on top, 2 on the bottom)—just enough for the joystick’s five signals: UP, DOWN, LEFT, RIGHT, and FIRE.

The firmware uses the BleGamepad library and presents itself as a `CONTROLLER_TYPE_JOYSTICK`.
It works out of the box on Windows with Stella, while on RetroPie, some additional configuration may be needed (such as RetroArch gamepad setup through its menu).

## Features
* Supports all 5 Atari 2600 Joystick buttons
* Bluetooth Advertising: Hold the FIRE button for 5 seconds to enter pairing mode.
* Power Saving: Automatically enters light sleep mode after 1 minute of inactivity (configurable).
