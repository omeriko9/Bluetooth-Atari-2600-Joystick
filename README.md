### ESP32/M5StackPlus based Atari 2600 classic joystick firmware

## Description
This project help taking an already existing Atari 2600 classic joystick, and with simple connections to its PCB make it wireless (Bluetooth) controller, which then in turn can be used in platforms such as RetroPie or others.

In order to achieve this, one needs to already have the Atari 2600 class joystick (clone or original) which can be bought quite cheaply at commercial websites.
For my project, I used M5StackPlus1.1, which has 5 exposed GPIO, 3 on top and 2 on the bottom.
This is enough since the Atari joystick also has 5 connections: UP DOWN LEFT RIGHT and FIRE.

The firmware exposes a `CONTROLLER_TYPE_JOYSTICK` using the `BleGamepad` library.
This works out of the box on Windows running Stella, but on RetroPie requires some further configurations (like retroarch gamepad configuration which can be done from the UI menus).

The firmware also handles Bluetooth advertisement (5 seconds press on the fire button), and some light sleep mode (after 1 minutes, configurable) that any key will wake up from.
