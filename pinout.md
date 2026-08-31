# Pinout

## Original PCB

| Original MCU pad | Function |
|---:|---|
| 1 | + battery, about 4.5 V from 3×AAA |
| 2 | unused / service connector |
| 3 | unused / service connector |
| 4 | unused |
| 5 | unused |
| 6 | motor control, active LOW |
| 7 | cover/mode sensor input |
| 8 | IR LED control for hand sensor |
| 9 | red LED, active LOW |
| 10 | unused |
| 11 | green LED, active LOW |
| 12 | blue LED, active LOW |
| 13 | unused |
| 14 | hand photodetector input |
| 15 | unused |
| 16 | GND |

## PIC16F1824 installation

The SOIC-14 PIC is shifted inward by one pad.

| PIC pin | PIC signal | Original PCB pad | Function |
|---:|---|---:|---|
| 1 | VDD | 2 | + supply; jumper old pad 1 -> 2 |
| 5 | RC5 | 6 | motor |
| 6 | RC4 | 7 | mode sensor |
| 7 | RC3 | 8 | IR LED |
| 8 | RC2 | 9 | red |
| 10 | RC0 | 11 | green |
| 11 | RA2 | 12 | blue |
| 13 | RA0/AN0 | 14 | photodetector |
| 14 | VSS | 15 | GND; jumper old pad 16 -> 15 |

## Signal polarity

- Motor: `0 = ON`, `1 = OFF`
- RGB LED channels: `0 = ON`, `1 = OFF`
