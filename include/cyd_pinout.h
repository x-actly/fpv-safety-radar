#ifndef CYD_PINOUT_H
#define CYD_PINOUT_H

#include <Arduino.h>

// ==========================================
// ESP32-2432S028R ("Cheap Yellow Display")
// Hardware Pinout Definitions
// ==========================================

// Display SPI Pins (ILI9341 240x320)
#define CYD_TFT_MISO 12
#define CYD_TFT_MOSI 13
#define CYD_TFT_SCLK 14
#define CYD_TFT_CS   15
#define CYD_TFT_DC   2
#define CYD_TFT_RST  -1  // Tied to EN
#define CYD_TFT_BL   21  // Backlight LED PWM pin

// Touch SPI Pins (XPT2046) - Uses separate SPI bus (VSPI)
#define CYD_TOUCH_MISO 39
#define CYD_TOUCH_MOSI 32
#define CYD_TOUCH_CLK  25
#define CYD_TOUCH_CS   33
#define CYD_TOUCH_IRQ  36

// Onboard RGB LED (Active LOW!)
#define CYD_LED_RED   4
#define CYD_LED_GREEN 16
#define CYD_LED_BLUE  17

// Onboard Peripherals
#define CYD_SPEAKER   26  // Audio PWM / DAC pin
#define CYD_LDR       34  // Light dependence resistor (ADC)

// Touch Screen Calibration Parameters for 240x320 Landscape (320x240)
#define CYD_TOUCH_MIN_X 300
#define CYD_TOUCH_MAX_X 3800
#define CYD_TOUCH_MIN_Y 300
#define CYD_TOUCH_MAX_Y 3800

// RGB LED Helper Macros (Active LOW)
inline void cydLedSet(bool red, bool green, bool blue) {
    pinMode(CYD_LED_RED, OUTPUT);
    pinMode(CYD_LED_GREEN, OUTPUT);
    pinMode(CYD_LED_BLUE, OUTPUT);
    digitalWrite(CYD_LED_RED, red ? LOW : HIGH);
    digitalWrite(CYD_LED_GREEN, green ? LOW : HIGH);
    digitalWrite(CYD_LED_BLUE, blue ? LOW : HIGH);
}

inline void cydLedOff() {
    cydLedSet(false, false, false);
}

inline void cydBeep(uint16_t freqHz = 2000, uint16_t durationMs = 100) {
    ledcAttachPin(CYD_SPEAKER, 0);
    ledcWriteTone(0, freqHz);
    delay(durationMs);
    ledcWriteTone(0, 0);
}

#endif // CYD_PINOUT_H
