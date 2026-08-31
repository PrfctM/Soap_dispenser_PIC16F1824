# Firmware logic

## Modes

The mode sensor cycles:

1. OFF -> cyan mode
2. cyan -> white mode
3. white -> OFF

Indication:

- cyan mode: green + blue
- white mode: red + green + blue
- OFF: red

## Dispensing

When enabled:

1. Optical sensor is armed only after it has seen a clear/no-hand state.
2. A new hand detection starts the motor immediately.
3. Removing the hand stops the motor.
4. If the hand remains present:
   - cyan mode stops after 1.2 s
   - white mode stops after 2.0 s
5. After timeout, the system waits for the hand to be removed before another dispense is allowed.

## Optical sensing

The IR LED is pulsed.

The firmware measures:

- photodetector level with IR off
- photodetector level with IR on

The absolute difference is used as the hand signal.

Main sensitivity constant:

```c
#define HAND_THRESHOLD 40
```

Increase this number if the dispenser detects a hand from too far away.

## Battery indication

The PIC measures VDD using the internal 1.024 V fixed voltage reference.

Approximate low-battery threshold is currently set to 3.3 V.

When low battery is detected after dispensing, the red LED flashes twice.
