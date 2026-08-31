# Soap_dispenser_PIC16F1824

Replacement firmware for an automatic soap dispenser originally using an unidentified 16-pin MCU.

The original controller is replaced by a **PIC16F1824-I/SL (SOIC-14)** installed with a one-pad shift on the original SOP-16 footprint.

## Current behavior

- Mode 1: cyan indication, maximum dispense time **1.2 s**
- Mode 2: white indication, maximum dispense time **2.0 s**
- Mode 3: red indication, dispenser OFF
- Hand detected -> motor starts
- Hand removed -> motor stops
- If hand remains present, motor stops at the mode timeout
- After timeout, dispenser will not restart until the hand is removed and presented again
- Low battery -> two short red flashes after dispensing

## Firmware

Source:

`firmware/main.c`

Compiler:

- MPLAB X IDE
- MPLAB XC8
- Device: PIC16F1824

## Important tuning

Hand sensor sensitivity is controlled by:

```c
#define HAND_THRESHOLD 40
```

Higher value = shorter detection distance / less sensitivity.

Suggested values if too sensitive:

`60`, `80`, `100`, `120`

## Repository status

This firmware is still under test on the real dispenser hardware.
