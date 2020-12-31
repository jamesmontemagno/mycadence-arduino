# arduino-cadence-display

This is an open source project to connect to popular cadence sensors for indoor cycling and display the current cadence in realtime. Inspired by https://github.com/snowzach/echbt

// IMAGE

## Hardware

* [MarkerFocus ESP32](https://amzn.to/2LAmqt4)


## Setup

1. Install [Arduino Studio](https://www.arduino.cc/en/software)
2. Open .ino in Arduino Studio
3. Add ESP32 Device - [Follow these steps](https://heltec-automation-docs.readthedocs.io/en/latest/esp32/quick_start.html)
4. Select Wifi 32 from Tools -> Board -> Heltec
5. Add ESP32 Library - [Follow these steps](https://github.com/HelTecAutomation/Heltec_ESP32)
6. Add ESP32 BLE Arduino Library - https://github.com/nkolban/ESP32_BLE_Arduino
7. Deploy to device!

The app will automatically scan for cadence sensors and start reporting data!


LICENSE: <a rel="license" href="http://creativecommons.org/licenses/by-nc-sa/4.0/"><img alt="Creative Commons License" style="border-width:0" src="https://i.creativecommons.org/l/by-nc-sa/4.0/80x15.png" /></a><br />This work is licensed under a <a rel="license" href="http://creativecommons.org/licenses/by-nc-sa/4.0/">Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License</a>.
