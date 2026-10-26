# ESP32 Smart Door Access System

A prototype smart door access system built around an ESP32 and simulated in Wokwi. The project explores keypad-based access control, local password storage, status indicators, motion sensing, and an audible alert after repeated incorrect password attempts.

> **Project status:** Prototype / work in progress. The servo's physical lock orientation and the reset-button wiring still need to be verified in the Wokwi simulation before this should be described as fully working. This is not a certified security system and should not be used to secure real property.

## Features implemented in the sketch

- First-time password setup using a 4×4 keypad
- Password confirmation before saving
- Local password persistence using the ESP32 `Preferences` (NVS) library
- Password entry using the keypad (`#` to submit, `*` to clear)
- Servo-controlled lock/unlock flow after a correct password
- Automatic relocking after a short unlock period
- Alarm sequence after three incorrect password attempts
- Buzzer and LED status feedback
- Optional PIR motion detection, toggled with the `A` key
- Physical password-reset flow designed around a button held for five seconds while the door is locked

## Hardware / Wokwi components

- ESP32 DevKit
- ILI9341 TFT display
- 4×4 membrane keypad
- Servo motor
- Buzzer
- PIR motion sensor
- Red, green, blue, and yellow LEDs with current-limiting resistors
- Reset pushbutton and external 10 kΩ pull-up resistor (must be added to the supplied diagram if not already present)

## Pin mapping

| Component | ESP32 GPIO |
|---|---:|
| TFT CS | 5 |
| TFT D/C | 2 |
| TFT RST | 4 |
| TFT MOSI | 23 |
| TFT SCK | 18 |
| Keypad rows R1–R4 | 32, 33, 25, 26 |
| Keypad columns C1–C4 | 27, 14, 12, 13 |
| Servo signal | 21 |
| Buzzer | 19 |
| Red LED | 16 |
| Green LED | 17 |
| Blue LED | 15 |
| Yellow LED | 0 |
| PIR output | 22 |
| Password reset button | 34 |

## Libraries

Install these libraries in the Arduino IDE or PlatformIO environment:

- Keypad
- Adafruit GFX Library
- Adafruit ILI9341
- ESP32Servo

`Arduino.h`, `SPI.h`, and `Preferences.h` are provided by the ESP32 Arduino core.

## Wokwi setup

1. Create or open an ESP32 project in Wokwi.
2. Add the supplied `ESP32_Smart_Door_Access.ino` sketch.
3. Copy your Wokwi circuit JSON into a file named `diagram.json` in the project folder.
4. Ensure the reset button is wired from GPIO34 to GND, with an external 10 kΩ pull-up from GPIO34 to 3.3 V. GPIO34 does not have an internal pull-up.
5. Run the simulation and test password setup, correct and incorrect passwords, alarm behavior, and reset behavior.
6. Verify the servo horn angle visually: the code currently treats 0° as locked and 90° as unlocked. Change those angles if your simulated mechanism is mounted in the opposite orientation.

## Keypad controls

- Digits `0`–`9`: enter a password
- `#`: submit / confirm
- `*`: clear the current entry
- `A`: toggle software motion detection
- `B`: intentionally ignored in this version

## Known limitations / next steps

- Confirm the servo's locked and unlocked angles for the actual simulated mechanism.
- Add and test the reset button in `diagram.json`.
- Test the complete sketch in Wokwi and resolve any compile/runtime issues before calling the prototype complete.
- The password is stored locally in ESP32 NVS; this prototype is not hardened against physical access or firmware extraction.
- Do not publish real Wi-Fi tokens, API keys, or other credentials in this repository.

## Author

Solomon Ezekiel
