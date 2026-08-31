# PROJECT_CONTEXT — Soap_dispenser_PIC16F1824

## Project goal

Replace the unidentified original 16-pin microcontroller in an automatic soap dispenser with a **PIC16F1824-I/SL (SOIC-14)**, reusing the original PCB, motor driver, IR hand sensor, RGB indicator LED, and lid/mode touch sensor.

The replacement PIC is installed on the original SOP-16 footprint with a **one-pad inward shift**.

Target toolchain:

- MCU: PIC16F1824-I/SL
- Package: SOIC-14
- Compiler: MPLAB XC8
- IDE: MPLAB X
- Programmer: TL866II Plus
- Power: 3×AAA alkaline cells, approximately 4.5 V nominal

Repository:

`PrfctM/Soap_dispenser_PIC16F1824`

---

## Confirmed original PCB pin functions

| Original MCU pad | Function | Confirmed behavior |
|---:|---|---|
| 1 | VCC | Battery positive, approx. 4.5 V |
| 2 | Service connector | Not needed |
| 3 | Service connector | Not needed |
| 4 | NC / unused | Not needed |
| 5 | NC / unused | Not needed |
| 6 | Motor control | **VCC = motor OFF, 0 V = motor ON** |
| 7 | Lid/mode sensor | Sensor used to cycle operating modes |
| 8 | IR LED drive | Drives IR emitter through transistor |
| 9 | Red LED channel | **0 = ON, VCC = OFF** |
| 10 | NC / unused | Not needed |
| 11 | Green LED channel | **0 = ON, VCC = OFF** |
| 12 | Blue LED channel | **0 = ON, VCC = OFF** |
| 13 | NC / unused | Not needed |
| 14 | Hand photodetector input | Short signal pulse appears when hand is present |
| 15 | NC / unused | Used as shifted PIC ground pad |
| 16 | GND | Battery negative |

---

## PIC16F1824 installation

PIC16F1824 SOIC-14 is shifted inward by one pad relative to the original SOP-16 footprint.

### Physical mapping

| PIC pin | PIC function | Original PCB pad | Board function |
|---:|---|---:|---|
| 1 | VDD | 2 | Supply; jumper old pad 1 → old pad 2 |
| 2 | RA5 | 3 | Unused |
| 3 | RA4/CPS3 | 4 | Available for capacitive touch if needed |
| 4 | RA3/MCLR/VPP | 5 | ICSP / reset |
| 5 | RC5 | 6 | Motor control |
| 6 | RC4 | 7 | Lid/mode sensor in current wiring |
| 7 | RC3 | 8 | IR LED control |
| 8 | RC2 | 9 | Red LED |
| 9 | RC1 | 10 | Unused |
| 10 | RC0 | 11 | Green LED |
| 11 | RA2 | 12 | Blue LED |
| 12 | RA1/ICSPCLK | 13 | ICSP clock |
| 13 | RA0/AN0/ICSPDAT | 14 | Hand photodetector ADC input |
| 14 | VSS | 15 | Ground; jumper old pad 16 → old pad 15 |

### Required jumpers

- Old pad **1 → 2** for VDD
- Old pad **16 → 15** for GND

No signal jumpers are needed with this shifted mounting.

---

## ICSP programming

PIC can be programmed in-circuit.

Relevant PIC pins:

- VPP/MCLR: PIC pin 4
- VDD: PIC pin 1
- VSS: PIC pin 14
- ICSPCLK/PGC: PIC pin 12 / RA1
- ICSPDAT/PGD: PIC pin 13 / RA0

The project currently uses TL866II Plus for programming.

---

## Lid / mode sensor

The lid sensor works well in the current hardware.

Observed behavior after programming the PIC:

- First touch changes mode and LED indication
- Second touch changes to the other active mode
- Third touch switches to OFF / red
- Sensor response itself is reported as excellent

Earlier discussion considered using the PIC capacitive-sensing module.

Important pin fact:

- **RC4 is not a CPS input**
- **RA4/CPS3 (PIC pin 3, old board pad 4)** is available if true hardware capacitive sensing is desired later

Current firmware uses the existing sensor signal on RC4 as a digital input.

---

## Operating modes — required behavior

Three-state cycle:

1. **Cyan mode**
   - Indicator: cyan
   - Maximum soap dispense time: **1.2 s**

2. **White mode**
   - Indicator: white
   - Maximum soap dispense time: **2.0 s**

3. **OFF mode**
   - Indicator: red
   - Hand sensor must not start the motor

Mode indication is shown briefly when changing modes, then the RGB LED may be turned off.

RGB channels are active-low.

### Color generation

- Red = red channel ON
- Cyan = green + blue ON
- White = red + green + blue ON

---

## Required hand-detection behavior

Correct behavior:

1. Dispenser is in cyan or white mode.
2. No hand present → system is armed and waiting.
3. **Hand enters detection area → motor starts immediately.**
4. **Hand removed before timeout → motor stops immediately.**
5. If hand stays present:
   - cyan mode stops motor after 1.2 s
   - white mode stops motor after 2.0 s
6. After timeout, motor must **not restart while the same hand remains present**.
7. Another dispense is allowed only after:
   - hand is removed
   - hand is presented again

The “clear/no-hand” state is only for arming. It must never itself start the motor.

---

## Hand sensor hardware

### IR emitter

Original board pad 8 / PIC RC3 controls the IR LED through a transistor.

Observed with multimeter on original hardware:

- A brief pulse occurs roughly periodically
- Meter showed about 1 V average/briefly, but actual pulse may be close to VCC because a multimeter cannot capture short pulses accurately

Current firmware assumes:

- `IR_ACTIVE_HIGH = 1`

This polarity may need confirmation.

### Photodetector

Original pad 14 / PIC RA0/AN0.

Observed:

- Normally meter reads ~0 V
- When hand is brought near, a very brief ~0.01 V indication can be seen on a multimeter
- This almost certainly does not represent the true instantaneous amplitude because the pulse is very short

Therefore the firmware uses ADC sampling and compares:

- photodetector level with IR OFF
- photodetector level with IR ON

The absolute difference is treated as the reflected-IR hand signal.

---

## Important test result from first firmware

The first programmed controller worked mechanically and the mode sensor behaved correctly, but optical hand detection was far too sensitive.

Observed problems:

1. Selecting cyan or white caused soap dispensing immediately.
2. Even while in red/OFF mode, the motor reportedly started once after about five minutes.
3. After later switching to white, the hand sensor reacted from approximately **0.5 m**, which is far too sensitive.

Likely cause:

- initial hand detection threshold was much too low (`HAND_THRESHOLD = 3`)
- optical algorithm treated noise / background difference as a hand

Planned correction:

- raise threshold substantially
- use averaging instead of maximum transient sample
- require a clear/no-hand state before arming after mode change
- force motor OFF continuously in OFF state

Current starting threshold in v2:

```c
#define HAND_THRESHOLD 40
```

Suggested tuning if still too sensitive:

```text
60
80
100
120
```

Higher value = less sensitive / shorter range.

Current release hysteresis:

```c
#define RELEASE_THRESHOLD 20
```

---

## Motor behavior

Motor control is active-low:

```text
0 = motor ON
1 = motor OFF
```

Safety requirement:

Outside the dedicated `dispense()` routine, firmware should continually force the motor output to OFF.

In OFF/red mode:

- motor must always be OFF
- IR emitter should be OFF
- hand detection should not trigger dispensing

If the motor ever starts in OFF mode after these software protections, investigate:

- PIC reset / brownout
- power disturbance from motor
- transient output state during reset
- noise on motor transistor gate/base
- missing pull-up on active-low motor line
- battery contact bounce
- watchdog / unintended reset behavior

A hardware pull-up on the motor-control transistor input may be worth considering so the motor remains OFF during PIC reset or high-impedance startup.

---

## Low battery behavior

Original dispenser behavior:

After soap dispensing stops, when batteries are weak:

- red LED flashes **twice briefly**

Target firmware reproduces this.

Current concept:

- use PIC internal 1.024 V FVR
- measure FVR with VDD as ADC reference
- infer supply voltage without a resistor divider

Current approximate threshold:

```c
#define LOW_BATTERY_ADC 317
```

This corresponds roughly to ~3.3 V total battery voltage.

This threshold should be validated on real hardware because motor load causes battery sag.

---

## Current firmware parameters

Target current values:

```c
#define HAND_THRESHOLD        40
#define RELEASE_THRESHOLD     20

#define HAND_CONFIRM_COUNT    3
#define RELEASE_CONFIRM_COUNT 2

#define SCAN_PERIOD_MS        40

#define MODE_CYAN_TIME_MS     1200UL
#define MODE_WHITE_TIME_MS    2000UL

#define ARM_RELEASE_COUNT     12

#define LOW_BATTERY_ADC       317
```

---

## Current source structure

Recommended repository structure:

```text
Soap_dispenser_PIC16F1824/
├── README.md
├── PROJECT_CONTEXT.md
├── .gitignore
├── firmware/
│   └── main.c
├── docs/
│   ├── logic.md
│   └── pinout.md
└── hex/
    └── README.md
```

The repository currently had several files uploaded at the root during manual setup, including:

- README.md
- main.c
- logic.md
- pinout.md
- an unwanted `download` file

These can be reorganized later.

---

## Current development priority

The next firmware iteration should focus on **reliable optical hand detection**.

Priority order:

1. Ensure OFF/red mode can never run the motor.
2. Confirm mode switching remains reliable.
3. Prevent immediate dispense after mode selection.
4. Tune optical threshold so normal detection range is approximately a few centimeters, not 0.5 m.
5. Confirm:
   - hand enters → motor ON
   - hand leaves → motor OFF
   - timeout at 1.2 s / 2.0 s
   - no repeat until hand is removed
6. Validate low-battery double-red flash.
7. After behavior is stable, reduce power consumption using sleep / lower duty-cycle optical scanning.

---

## Notes for Codex

When modifying `main.c`:

- Preserve the confirmed shifted pinout.
- Do not change motor polarity.
- Do not change RGB polarity.
- Treat RA0 as the photodetector ADC input.
- Do not let “sensor clear” trigger the motor.
- A dispense must begin only on a transition to **hand present** while armed.
- After timeout, wait for a full hand removal before allowing another dispense.
- OFF mode must be fail-safe.
- Keep tunable constants grouped near the top of the file.
- Prefer simple, testable state-machine logic over deeply nested blocking loops.
- Comment hardware assumptions clearly.
- If modifying the capacitive lid sensor, remember RC4 is not a CPS channel; RA4/CPS3 is available.

---

## Known repository access issue

The GitHub repository belongs to `PrfctM`.

A collaborator account `wbl100500-bot` was added and repository metadata showed `push=true`, but the ChatGPT GitHub integration still returned:

`403 Resource not accessible by integration`

for write operations.

Therefore Codex or direct Git access may be needed for commits/pushes.

---

## Summary of validated hardware status

Validated on real board:

- PIC replacement physically works
- power mapping works
- motor output works
- RGB channels work
- lid/mode sensor works very well
- optical hardware responds
- main remaining issue is optical sensitivity / false triggering
