#include <xc.h>
#include <stdint.h>
#include <stdbool.h>

#define _XTAL_FREQ 4000000UL

// ============================================================
// CONFIG
// ============================================================

#pragma config FOSC = INTOSC
#pragma config WDTE = OFF
#pragma config PWRTE = ON
#pragma config MCLRE = ON
#pragma config CP = OFF
#pragma config CPD = OFF
#pragma config BOREN = ON
#pragma config CLKOUTEN = OFF
#pragma config IESO = OFF
#pragma config FCMEN = OFF

#pragma config WRT = OFF
#pragma config PLLEN = OFF
#pragma config STVREN = ON
#pragma config BORV = LO
#pragma config LVP = OFF

// ============================================================
// BOARD CONNECTIONS
//
// PIC16F1824 SOIC-14 shifted by one pad on original SOP-16 PCB:
//
// PIC 1  VDD -> old pad 2 -> jumper to old pad 1 (+ battery)
// PIC 5  RC5 -> old pad 6  MOTOR
// PIC 6  RC4 -> old pad 7  MODE SENSOR
// PIC 7  RC3 -> old pad 8  IR LED
// PIC 8  RC2 -> old pad 9  RED LED
// PIC 10 RC0 -> old pad 11 GREEN LED
// PIC 11 RA2 -> old pad 12 BLUE LED
// PIC 13 RA0 -> old pad 14 PHOTO SENSOR
// PIC 14 VSS -> old pad 15 -> jumper to old pad 16 (GND)
//
// NOTE:
// If the cover sensor is converted to real capacitive sensing,
// route the sensor electrode to RA4/CPS3 instead of RC4.
// This source currently uses the existing digital MODE_SENSOR input.
//
// ============================================================

#define MOTOR       LATCbits.LATC5
#define IR_LED      LATCbits.LATC3

#define LED_RED     LATCbits.LATC2
#define LED_GREEN   LATCbits.LATC0
#define LED_BLUE    LATAbits.LATA2

#define MODE_SENSOR PORTCbits.RC4

// ============================================================
// SETTINGS
// ============================================================

// Motor: 0 = ON, 1 = OFF
#define MOTOR_ON_LEVEL        0
#define MOTOR_OFF_LEVEL       1

// RGB LEDs: 0 = ON, 1 = OFF
#define LED_ON_LEVEL          0
#define LED_OFF_LEVEL         1

// Cover/mode sensor: 0 = finger present, 1 = no finger
#define MODE_ACTIVE_LEVEL     0

// IR transmitter polarity
#define IR_ACTIVE_HIGH        1

// Hand sensor sensitivity.
// Higher = less sensitive.
// Start at 40. If still too sensitive, try 60, 80, 100...
#define HAND_THRESHOLD        40

// Hysteresis threshold for hand removal
#define RELEASE_THRESHOLD     20

#define HAND_CONFIRM_COUNT    3
#define RELEASE_CONFIRM_COUNT 2

// Optical scan period
#define SCAN_PERIOD_MS        40

// Dispense times
#define MODE_CYAN_TIME_MS     1200UL
#define MODE_WHITE_TIME_MS    2000UL

// After mode change, sensor must see "no hand" repeatedly before arming
#define ARM_RELEASE_COUNT     12

// Approx. 3.3 V low battery threshold using internal 1.024 V FVR
#define LOW_BATTERY_ADC       317

typedef enum
{
    MODE_OFF = 0,
    MODE_CYAN,
    MODE_WHITE
} dispenser_mode_t;

// ============================================================
// IR CONTROL
// ============================================================

static void ir_on(void)
{
#if IR_ACTIVE_HIGH
    IR_LED = 1;
#else
    IR_LED = 0;
#endif
}

static void ir_off(void)
{
#if IR_ACTIVE_HIGH
    IR_LED = 0;
#else
    IR_LED = 1;
#endif
}

// ============================================================
// MOTOR
// ============================================================

static void motor_on(void)
{
    MOTOR = MOTOR_ON_LEVEL;
}

static void motor_off(void)
{
    MOTOR = MOTOR_OFF_LEVEL;
}

// ============================================================
// LEDS
// ============================================================

static void leds_off(void)
{
    LED_RED   = LED_OFF_LEVEL;
    LED_GREEN = LED_OFF_LEVEL;
    LED_BLUE  = LED_OFF_LEVEL;
}

static void red_on(void)
{
    leds_off();
    LED_RED = LED_ON_LEVEL;
}

static void cyan_on(void)
{
    leds_off();
    LED_GREEN = LED_ON_LEVEL;
    LED_BLUE  = LED_ON_LEVEL;
}

static void white_on(void)
{
    LED_RED   = LED_ON_LEVEL;
    LED_GREEN = LED_ON_LEVEL;
    LED_BLUE  = LED_ON_LEVEL;
}

// ============================================================
// ADC
// ============================================================

static uint16_t adc_convert(void)
{
    __delay_us(20);

    ADCON0bits.GO_nDONE = 1;
    while (ADCON0bits.GO_nDONE)
    {
    }

    return (((uint16_t)ADRESH << 8) | ADRESL);
}

static uint16_t adc_read_photo(void)
{
    ADCON0bits.CHS = 0b00000; // AN0
    return adc_convert();
}

// ============================================================
// HAND SENSOR
// ============================================================

static uint16_t read_hand_signal(void)
{
    uint32_t off_sum = 0;
    uint32_t on_sum  = 0;

    uint16_t off_avg;
    uint16_t on_avg;

    uint8_t i;

    // Ambient light measurement
    ir_off();
    __delay_us(150);

    for (i = 0; i < 4; i++)
    {
        off_sum += adc_read_photo();
        __delay_us(60);
    }

    off_avg = (uint16_t)(off_sum >> 2);

    // IR illuminated measurement
    ir_on();
    __delay_us(100);

    for (i = 0; i < 4; i++)
    {
        on_sum += adc_read_photo();
        __delay_us(60);
    }

    ir_off();

    on_avg = (uint16_t)(on_sum >> 2);

    if (on_avg >= off_avg)
        return (on_avg - off_avg);
    else
        return (off_avg - on_avg);
}

static bool hand_present(void)
{
    static bool state = false;
    static uint8_t on_count = 0;
    static uint8_t off_count = 0;

    uint16_t signal = read_hand_signal();

    if (!state)
    {
        if (signal >= HAND_THRESHOLD)
        {
            if (on_count < HAND_CONFIRM_COUNT)
                on_count++;

            if (on_count >= HAND_CONFIRM_COUNT)
            {
                state = true;
                on_count = 0;
                off_count = 0;
            }
        }
        else
        {
            on_count = 0;
        }
    }
    else
    {
        if (signal <= RELEASE_THRESHOLD)
        {
            if (off_count < RELEASE_CONFIRM_COUNT)
                off_count++;

            if (off_count >= RELEASE_CONFIRM_COUNT)
            {
                state = false;
                on_count = 0;
                off_count = 0;
            }
        }
        else
        {
            off_count = 0;
        }
    }

    return state;
}

// ============================================================
// BATTERY
// ============================================================

static uint16_t read_battery_fvr_adc(void)
{
    uint16_t value;

    // FVREN = 1, ADFVR = 01 => 1.024 V
    FVRCON = 0x81;
    __delay_ms(2);

    // Internal FVR ADC channel
    ADCON0bits.CHS = 0b11111;
    __delay_us(40);

    value = adc_convert();

    ADCON0bits.CHS = 0b00000;
    FVRCON = 0x00;

    return value;
}

static bool battery_low(void)
{
    return (read_battery_fvr_adc() >= LOW_BATTERY_ADC);
}

static void low_battery_flash(void)
{
    uint8_t i;

    for (i = 0; i < 2; i++)
    {
        red_on();
        __delay_ms(180);
        leds_off();
        __delay_ms(180);
    }
}

// ============================================================
// MODE INDICATION
// ============================================================

static void show_mode(dispenser_mode_t mode)
{
    if (mode == MODE_CYAN)
    {
        cyan_on();
        __delay_ms(700);
    }
    else if (mode == MODE_WHITE)
    {
        white_on();
        __delay_ms(700);
    }
    else
    {
        red_on();
        __delay_ms(700);
    }

    leds_off();
}

// ============================================================
// MODE SENSOR
// ============================================================

static bool mode_touch_event(void)
{
    if (MODE_SENSOR == MODE_ACTIVE_LEVEL)
    {
        __delay_ms(30);

        if (MODE_SENSOR == MODE_ACTIVE_LEVEL)
        {
            while (MODE_SENSOR == MODE_ACTIVE_LEVEL)
            {
                motor_off();
                __delay_ms(10);
            }

            __delay_ms(30);
            return true;
        }
    }

    return false;
}

// ============================================================
// ARMING
// ============================================================

static bool wait_sensor_clear_nonblocking(uint8_t *counter)
{
    bool hand = hand_present();

    if (!hand)
    {
        if (*counter < ARM_RELEASE_COUNT)
            (*counter)++;

        if (*counter >= ARM_RELEASE_COUNT)
            return true;
    }
    else
    {
        *counter = 0;
    }

    return false;
}

// ============================================================
// DISPENSE
// ============================================================

static void dispense(uint16_t maximum_time_ms)
{
    uint16_t elapsed = 0;
    uint8_t release_count = 0;

    motor_on();

    while (elapsed < maximum_time_ms)
    {
        // Stop immediately if mode sensor is touched
        if (MODE_SENSOR == MODE_ACTIVE_LEVEL)
            break;

        // Stop quickly when hand is removed
        if (!hand_present())
        {
            release_count++;

            if (release_count >= 2)
                break;
        }
        else
        {
            release_count = 0;
        }

        __delay_ms(SCAN_PERIOD_MS);
        elapsed += SCAN_PERIOD_MS;
    }

    motor_off();
    ir_off();

    __delay_ms(80);

    if (battery_low())
        low_battery_flash();
}

// ============================================================
// INIT
// ============================================================

static void init_pic(void)
{
    // Internal oscillator 4 MHz
    OSCCONbits.IRCF = 0b1101;
    OSCCONbits.SCS  = 0b10;

    // Safe output states first
    LATCbits.LATC5 = MOTOR_OFF_LEVEL;

#if IR_ACTIVE_HIGH
    LATCbits.LATC3 = 0;
#else
    LATCbits.LATC3 = 1;
#endif

    LATCbits.LATC2 = LED_OFF_LEVEL;
    LATCbits.LATC0 = LED_OFF_LEVEL;
    LATAbits.LATA2 = LED_OFF_LEVEL;

    // Only RA0 analog
    ANSELA = 0b00000001;
    ANSELC = 0b00000000;

    // RA0 input, RA2 output
    TRISA = 0b00000001;

    // RC4 input; RC5/RC3/RC2/RC0 outputs
    TRISC = 0b00010000;

    // ADC setup
    ADCON1bits.ADFM   = 1;
    ADCON1bits.ADPREF = 0b00;
    ADCON1bits.ADNREF = 0;
    ADCON1bits.ADCS   = 0b010;

    ADCON0bits.CHS  = 0b00000;
    ADCON0bits.ADON = 1;

    motor_off();
    ir_off();
    leds_off();

    __delay_ms(100);
}

// ============================================================
// MAIN
// ============================================================

void main(void)
{
    dispenser_mode_t mode = MODE_OFF;

    bool armed = false;
    bool hand;

    uint8_t clear_counter = 0;

    init_pic();

    while (1)
    {
        // Outside dispense(), motor must always be OFF
        motor_off();

        // Mode switching
        if (mode_touch_event())
        {
            motor_off();
            ir_off();

            if (mode == MODE_OFF)
                mode = MODE_CYAN;
            else if (mode == MODE_CYAN)
                mode = MODE_WHITE;
            else
                mode = MODE_OFF;

            show_mode(mode);

            // Require clear optical sensor before allowing dispense
            armed = false;
            clear_counter = 0;

            continue;
        }

        // OFF mode: motor and IR are always off
        if (mode == MODE_OFF)
        {
            motor_off();
            ir_off();

            armed = false;
            clear_counter = 0;

            __delay_ms(20);
            continue;
        }

        // Arming: wait until sensor clearly sees "no hand"
        if (!armed)
        {
            motor_off();

            if (wait_sensor_clear_nonblocking(&clear_counter))
            {
                armed = true;
                clear_counter = 0;
            }

            __delay_ms(SCAN_PERIOD_MS);
            continue;
        }

        // Ready: hand appears -> start dispensing immediately
        hand = hand_present();

        if (hand)
        {
            // Prevent repeat until hand is removed
            armed = false;
            clear_counter = 0;

            if (mode == MODE_CYAN)
                dispense(MODE_CYAN_TIME_MS);
            else if (mode == MODE_WHITE)
                dispense(MODE_WHITE_TIME_MS);
        }

        __delay_ms(SCAN_PERIOD_MS);
    }
}
