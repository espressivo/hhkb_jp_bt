// HHKB JP Keyboard Controller sketch for Arduino 
// Reference:
//  https://github.com/addsict/YetAnotherHHKBController/blob/master/hhkb/hhkb.ino
//  https://github.com/tmk/tmk_keyboard/blob/master/keyboard/hhkb/doc/HHKB.txt

#include <avr/sleep.h>
#include <avr/power.h>

#define MAX_ROLLOVER 6
#define MAX_MODIFIERS 6
#define MAX_ROWS 16
#define MAX_COLS 5

#define KEY_PRESSED LOW
#define COL_ENABLE LOW
#define COL_DISABLE HIGH

#define NONE 0
#define MOD 0

// #define DEBUG

// Arduino Pins
int muxRowControlPin[] = {4, 5, 6, 14, 15};
int muxColControlPin[] = {10, 9, 11};
int enableColPin = 12;
int keyReadPin = 3;
int hisPin = 2;

int histeresis;

// for Multiplexer
int bits[16][5] = {
    {0, 0, 0, 1, 0}, {1, 0, 0, 1, 0}, {0, 1, 0, 1, 0}, {1, 1, 0, 1, 0},
    {0, 0, 1, 1, 0}, {1, 0, 1, 1, 0}, {0, 1, 1, 1, 0}, {1, 1, 1, 1, 0},
    {0, 0, 0, 0, 1}, {1, 0, 0, 0, 1}, {0, 1, 0, 0, 1}, {1, 1, 0, 0, 1},
    {0, 0, 1, 0, 1}, {1, 0, 1, 0, 1}, {0, 1, 1, 0, 1}, {1, 1, 1, 0, 1}
};

int LFnKeyPos[2] = {0, 4};
int RFnKeyPos[2] = {14, 4};
int modifierKeyPos[MAX_MODIFIERS][3] = {
  {0, 6, bit(0)}, {0, 5, bit(1)}, {2, 4, bit(2)},  // LCr,  LSh,  LAL,
  {6, 4, bit(3)}, {12, 5, bit(5)}, {11, 4, bit(6)} // LGui, RSh,  RAL
};

uint8_t KEYCODE[2][MAX_ROWS * MAX_COLS] = {
    // RN-42(BT module) cannot send keycodes of (MHK, HK, KAN, ro, \) (>0x65).
    // Instead, send unused keycode(keypad 1-5, 0x59-0x5d),
    // and translate on Windows registry(Scancode map).
    {
        0x29, 0x2b, MOD,  MOD,  MOD,  // ESC, TAB, LFn, LSh, LCr,
        0x21, 0x08, 0x59, 0x06, 0x07, // 4,   e,   MHK, c,   d,
        0x20, 0x1a, MOD,  0x1b, 0x16, // 3,   w,   LAL, x,   s,
        0x1e, NONE, 0x35, NONE, NONE, // 1,        HHK,
        NONE, NONE, NONE, NONE, NONE, // 
        0x22, 0x15, NONE, 0x19, 0x09, // 5,   r,        v,   f,
        0x1f, 0x14, MOD,  0x1d, 0x04, // 2,   q,   LGu, z,   a,
        0x23, 0x17, 0x2c, 0x05, 0x0a, // 6,   t,   Spc, b,   g,
        0x26, 0x0c, 0x5b, 0x36, 0x0e, // 9,   i,   KAN, ,,   k,
        0x25, 0x18, 0x5a, 0x10, 0x0d, // 8,   u,   HK,  m,   j,
        0x24, 0x1c, NONE, 0x11, 0x0b, // 7,   y,        n,   h,
        0x27, 0x12, MOD,  0x37, 0x0f, // 0,   o,   RAL, .,   l,
        0x2a, NONE, 0x4f, MOD,  0x28, // BS,       ->,  RSh, Ent,
        0x5d, 0x30, 0x51, 0x52, 0x32, // \,   [,   Dwn, Up,  ],
        0x2d, 0x13, MOD,  0x38, 0x33, // -,   p,   RFn, /,   ;,
        0x2e, 0x2f, 0x50, 0x5c, 0x34, // ^,   @,   <-,  ro,  :,
    },
    // keys with prefix * are no use
    {
        NONE, NONE, NONE, NONE, NONE, // *Pwr, *Cap,
        0x3d, NONE, NONE, NONE, NONE, // F4,
        0x3c, NONE, NONE, NONE, NONE, // F3,
        0x3a, NONE, NONE, NONE, NONE, // F1,
        NONE, NONE, NONE, NONE, NONE, // 
        0x3e, NONE, NONE, NONE, NONE, // F5,
        0x3b, NONE, NONE, NONE, NONE, // F2,
        0x3f, NONE, NONE, NONE, NONE, // F6,
        0x42, NONE, NONE, NONE, NONE, // F9,
        0x41, NONE, NONE, NONE, NONE, // F8,
        0x40, NONE, NONE, NONE, NONE, // F7,
        0x43, NONE, NONE, NONE, NONE, // F10,
        0x4c, NONE, NONE, NONE, NONE, // Del,
        NONE, NONE, NONE, NONE, NONE, // *Ins,
        0x44, NONE, NONE, NONE, NONE, // F11,
        0x45, NONE, NONE, NONE, NONE, // F12,
    }
};

void setup() {
    Serial.begin(9600);
    // set pin mode
    int i;
    for (i = 0; i < 5; i++) {
        pinMode(muxRowControlPin[i], OUTPUT);
        digitalWrite(muxRowControlPin[i], LOW); 
    }
    for (i = 0; i < 3; i++) {
        pinMode(muxColControlPin[i], OUTPUT);
        digitalWrite(muxColControlPin[i], LOW);
    }
    pinMode(enableColPin, OUTPUT);
    digitalWrite(enableColPin, COL_DISABLE);
    pinMode(hisPin, OUTPUT);
    digitalWrite(hisPin, LOW);
    pinMode(keyReadPin, INPUT);

    // timer setting(64Hz = 16MHz / 1024 / 256)
    cli();
    TCCR2A = 0;
    TCCR2B = 0;
    TCNT2 = 0;
    TCCR2B = bit(CS22) | bit(CS20); // f = clock(16MHz) / 1024
    TIMSK2 = bit(TOIE2); // overflow(f = 256)
    set_sleep_mode(SLEEP_MODE_PWR_SAVE);
    sei();
}

void loop() {
    uint8_t modifiers = 0x00;
    uint8_t keys[MAX_ROLLOVER] = {0};
    int fnFlag = 0;
    int numKeys = 0;
    int keycode;

    // check Fn key and modifier keys
    if (isKeyPressed(LFnKeyPos[0], LFnKeyPos[1]) || isKeyPressed(RFnKeyPos[0], RFnKeyPos[1])) {
        fnFlag = 1;
    }
    for (int i = 0; i < MAX_MODIFIERS; i += 1) {
        if (isKeyPressed(modifierKeyPos[i][0], modifierKeyPos[i][1])) {
            modifiers |= modifierKeyPos[i][2];
        }
    }

    // check normal keys
    for (int row = 0; row < MAX_ROWS && numKeys < MAX_ROLLOVER; row += 1) {
        for (int col = 0; col < MAX_COLS; col += 1) {
            keycode = KEYCODE[fnFlag][row * MAX_COLS + col];
            if (keycode != NONE) {
                if (isKeyPressed(row, col+2)) {
                    keys[numKeys++] = keycode;
#ifdef DEBUG
                    Serial.println(keycode);
#endif
                    if (numKeys >= MAX_ROLLOVER) break;
                }
            }
        }
    }
    sendKeyCodes(modifiers, keys);

    Serial.flush();
    sleep_enable();
    sleep_cpu();
}

ISR (TIMER2_OVF_vect) {
    sleep_disable();
}

int isKeyPressed(int row, int col) {
    int i;
    for (i = 0; i < 5; i += 1) {
        digitalWrite(muxRowControlPin[i], bits[row][i]);
    }
    for (i = 0; i < 3; i += 1) {
        digitalWrite(muxColControlPin[i], bits[col][i]);
    }
    delayMicroseconds(5);
    digitalWrite(hisPin, histeresis);
    delayMicroseconds(10);
    digitalWrite(enableColPin, COL_ENABLE);
    delayMicroseconds(10);
    int result = digitalRead(keyReadPin);
    digitalWrite(hisPin, LOW);
    digitalWrite(enableColPin, COL_DISABLE);

    histeresis = (result == KEY_PRESSED) ? HIGH : LOW;
    return histeresis;
}

void sendKeyCodes(uint8_t modifiers, uint8_t keys[MAX_ROLLOVER]) { 
    // RN-42 format
    // See page 64 of http://ww1.microchip.com/downloads/en/DeviceDoc/bluetooth_cr_UG-v1.0r.pdf
    Serial.write(0xFD); // Raw Report Mode
    Serial.write(0x09); // Length
    Serial.write(0x01); // Descriptor 0x01=Keyboard
    Serial.write(modifiers); // modifier keys
    Serial.write(0x00);   // reserved
    Serial.write(keys, MAX_ROLLOVER); // keycode
}
