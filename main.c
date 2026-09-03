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
// PIC16F1824 SOIC-14 shifted by one pad on old SOP-16:
//
// PIC 1  VDD -> old pad 2 -> jumper to old pad 1 (+ battery)
// PIC 5  RC5 -> old pad 6  MOTOR
// PIC 6  RC4 -> old pad 7  MODE SENSOR
// PIC 7  RC3 -> old pad 8  IR LED
// PIC 8  RC2 -> old pad 9  RED
// PIC 10 RC0 -> old pad 11 GREEN
// PIC 11 RA2 -> old pad 12 BLUE
// PIC 13 RA0 -> old pad 14 PHOTO
// PIC 14 VSS -> old pad 15 -> jumper to old pad 16 GND
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

// Motor is active LOW
#define MOTOR_ON_LEVEL       0
#define MOTOR_OFF_LEVEL      1

// RGB channels are active LOW
#define LED_ON_LEVEL         0
#define LED_OFF_LEVEL        1

// Mode sensor:
// VCC = no finger
// 0V  = finger present
#define MODE_ACTIVE_LEVEL    0

/*
 * IR transmitter polarity.
 * Keep exactly as in the previous working version.
 */
#define IR_ACTIVE_HIGH       1

/*
 * PHOTO sensitivity.
 * Previous working value: 3.
 * New value: 4 = approximately 30% lower sensitivity / shorter range.
 */
#define HAND_THRESHOLD       4

#define HAND_CONFIRM_COUNT   2
#define RELEASE_CONFIRM_COUNT 2

#define SCAN_PERIOD_MS       50

/*
 * Anti-repeat protection:
 * after one dispense, another dispense is forbidden until
 * 1) a minimum lockout time has elapsed, AND
 * 2) the optical sensor has been continuously clear for REARM_CLEAR_MS.
 * This prevents foam left under the nozzle from being interpreted as a new hand.
 */
#define POST_DISPENSE_LOCKOUT_MS 1000UL
#define REARM_CLEAR_MS            700UL
#define LOCKOUT_SCAN_COUNT        ((uint8_t)(POST_DISPENSE_LOCKOUT_MS / SCAN_PERIOD_MS))
#define REARM_CLEAR_COUNT         ((uint8_t)(REARM_CLEAR_MS / SCAN_PERIOD_MS))

// ONLY CHANGES IN THIS REVISION:
//
// Mode 1 = cyan = 0.5 s
#define MODE1_TIME_MS        500UL

// Mode 2 = white = 0.9 s
#define MODE2_TIME_MS        900UL

/*
 * Low battery threshold.
 * Approx. 3.3V for 3xAAA.
 */
#define LOW_BATTERY_ADC      317

// ============================================================
// MODES
// ============================================================

typedef enum
{
    MODE_OFF = 0,
    MODE_2SEC,
    MODE_3SEC
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
// RGB LED
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

static void blue_on(void)
{
    leds_off();
    LED_BLUE = LED_ON_LEVEL;
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
    __delay_us(25);

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
// PHOTO / HAND SENSOR
// ============================================================

static uint16_t read_hand_signal(void)
{
    uint16_t background;
    uint16_t illuminated;
    uint16_t diff;
    uint16_t max_diff = 0;
    uint8_t i;

    ir_off();
    __delay_us(200);

    background = adc_read_photo();

    ir_on();

    for (i = 0; i < 6; i++)
    {
        __delay_us(100);

        illuminated = adc_read_photo();

        if (illuminated >= background)
        {
            diff = illuminated - background;
        }
        else
        {
            diff = background - illuminated;
        }

        if (diff > max_diff)
        {
            max_diff = diff;
        }
    }

    ir_off();

    return max_diff;
}

// ============================================================
// HAND DETECTION WITH FILTERING
// ============================================================

static bool hand_present(void)
{
    static uint8_t detect_count = 0;
    static uint8_t release_count = 0;
    static bool state = false;

    uint16_t signal;

    signal = read_hand_signal();

    if (signal >= HAND_THRESHOLD)
    {
        release_count = 0;

        if (detect_count < HAND_CONFIRM_COUNT)
        {
            detect_count++;
        }

        if (detect_count >= HAND_CONFIRM_COUNT)
        {
            state = true;
        }
    }
    else
    {
        detect_count = 0;

        if (release_count < RELEASE_CONFIRM_COUNT)
        {
            release_count++;
        }

        if (release_count >= RELEASE_CONFIRM_COUNT)
        {
            state = false;
        }
    }

    return state;
}

// ============================================================
// BATTERY MEASUREMENT USING INTERNAL FVR
// ============================================================

static uint16_t read_battery_fvr_adc(void)
{
    uint16_t value;

    FVRCON = 0x81;

    __delay_ms(2);

    ADCON0bits.CHS = 0b11111;

    __delay_us(50);

    value = adc_convert();

    ADCON0bits.CHS = 0b00000;

    FVRCON = 0x00;

    return value;
}

static bool battery_low(void)
{
    uint16_t value;

    value = read_battery_fvr_adc();

    if (value >= LOW_BATTERY_ADC)
    {
        return true;
    }

    return false;
}

// ============================================================
// LOW BATTERY INDICATION
// ============================================================

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
    switch (mode)
    {
        case MODE_2SEC:

            cyan_on();
            __delay_ms(700);
            leds_off();

            break;

        case MODE_3SEC:

            white_on();
            __delay_ms(700);
            leds_off();

            break;

        default:

            red_on();
            __delay_ms(700);
            leds_off();

            break;
    }
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
                __delay_ms(10);
            }

            __delay_ms(30);

            return true;
        }
    }

    return false;
}

// ============================================================
// DISPENSE
// ============================================================

static void dispense(uint32_t maximum_time_ms)
{
    uint32_t elapsed = 0;
    uint8_t hand_release_count = 0;

    motor_on();

    while (elapsed < maximum_time_ms)
    {
        if (!hand_present())
        {
            if (hand_release_count < 3)
            {
                hand_release_count++;
            }

            if (hand_release_count >= 3)
            {
                break;
            }
        }
        else
        {
            hand_release_count = 0;
        }

        __delay_ms(SCAN_PERIOD_MS);

        elapsed += SCAN_PERIOD_MS;
    }

    motor_off();

    ir_off();

    __delay_ms(100);

    if (battery_low())
    {
        low_battery_flash();
    }
}

// ============================================================
// INITIALIZATION
// ============================================================

static void init_pic(void)
{
    OSCCONbits.IRCF = 0b1101;
    OSCCONbits.SCS  = 0b10;

    MOTOR = MOTOR_OFF_LEVEL;

    LED_RED   = LED_OFF_LEVEL;
    LED_GREEN = LED_OFF_LEVEL;
    LED_BLUE  = LED_OFF_LEVEL;

#if IR_ACTIVE_HIGH
    IR_LED = 0;
#else
    IR_LED = 1;
#endif

    ANSELA = 0b00000001;
    ANSELC = 0b00000000;

    TRISA = 0b00000001;

    TRISC = 0b00010000;

    ADCON1bits.ADFM = 1;
    ADCON1bits.ADPREF = 0b00;
    ADCON1bits.ADNREF = 0;
    ADCON1bits.ADCS = 0b010;

    ADCON0bits.CHS = 0b00000;
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

    bool hand;
    bool armed = true;

    uint8_t detect_counter = 0;
    uint8_t clear_counter = 0;
    uint8_t lockout_counter = 0;

    init_pic();

    while (1)
    {
        if (mode_touch_event())
        {
            motor_off();

            /* Changing mode also resets the hand-detection state. */
            armed = true;
            detect_counter = 0;
            clear_counter = 0;
            lockout_counter = 0;

            if (mode == MODE_OFF)
            {
                mode = MODE_2SEC;
            }
            else if (mode == MODE_2SEC)
            {
                mode = MODE_3SEC;
            }
            else
            {
                mode = MODE_OFF;
            }

            show_mode(mode);

            __delay_ms(200);

            continue;
        }

        if (mode == MODE_OFF)
        {
            motor_off();
            ir_off();

            armed = true;
            detect_counter = 0;
            clear_counter = 0;
            lockout_counter = 0;

            __delay_ms(20);

            continue;
        }

        hand = hand_present();

        /*
         * DISARMED STATE (after dispensing):
         * Never start the motor here, even if foam reflects the IR beam.
         * First wait through the mandatory lockout, then require the sensor
         * to be continuously clear before arming the dispenser again.
         */
        if (!armed)
        {
            detect_counter = 0;

            if (lockout_counter > 0)
            {
                lockout_counter--;
                clear_counter = 0;
            }
            else
            {
                if (!hand)
                {
                    if (clear_counter < REARM_CLEAR_COUNT)
                    {
                        clear_counter++;
                    }

                    if (clear_counter >= REARM_CLEAR_COUNT)
                    {
                        armed = true;
                        clear_counter = 0;
                    }
                }
                else
                {
                    /* Foam/hand is still visible: stay locked indefinitely. */
                    clear_counter = 0;
                }
            }

            __delay_ms(SCAN_PERIOD_MS);
            continue;
        }

        /* ARMED STATE: normal hand detection. */
        if (hand)
        {
            if (detect_counter < 3)
            {
                detect_counter++;
            }

            if (detect_counter >= 3)
            {
                /* Disarm BEFORE running the motor so no second dose can occur. */
                armed = false;
                detect_counter = 0;
                clear_counter = 0;
                lockout_counter = LOCKOUT_SCAN_COUNT;

                if (mode == MODE_2SEC)
                {
                    dispense(MODE1_TIME_MS);
                }
                else
                {
                    dispense(MODE2_TIME_MS);
                }
            }
        }
        else
        {
            detect_counter = 0;
        }

        __delay_ms(SCAN_PERIOD_MS);
    }
}
