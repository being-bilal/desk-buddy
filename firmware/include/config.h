// ---------------------------------------------------------------------------
//  Desk Buddy - central configuration
//  Target: ESP32-C3 Super Mini (4 MB flash, native USB-Serial/JTAG)
// ---------------------------------------------------------------------------
#pragma once

#include <stdint.h>

// ------------------------------- identity ---------------------------------
#define FW_NAME          "Desk Buddy"
#define FW_VERSION       "0.1.0"
#define MDNS_HOSTNAME    "buddy"          // http://buddy.local

// ------------------------------ pin map -----------------------------------
// ESP32-C3 notes:
//   * No capacitive-touch peripheral and no DAC - hence the external TTP223
//     touch module and PWM-driven sound.
//   * GPIO8 is the on-board LED (active LOW), GPIO9 is the BOOT button.
//   * GPIO18/19 are the native USB D-/D+ and are not broken out.
//
//  *** STRAPPING PINS - read before changing PIN_BUZZER ***
//
//  GPIO2, GPIO8 and GPIO9 are sampled at every reset:
//
//     | strapping pin | SPI Boot (normal) | Download Boot |
//     |---------------|-------------------|---------------|
//     | GPIO2         | 1                 | 1             |
//     | GPIO8         | irrelevant        | 1             |
//     | GPIO9         | 1 (internal PU)   | 0             |
//
//  GPIO2 must therefore be HIGH at every reset, and unlike GPIO9 it has no
//  internal pull-up. Nothing attached to it may hold the pin low:
//    * a bare piezo disc is capacitive and harmless;
//    * a buzzer *module* with a pulldown on its input can stop the board
//      booting entirely.
//  If the board misbehaves at power-on, move the buzzer to GPIO6/7/10/20/21
//  and fit a 10k pull-up from GPIO2 to 3V3.
#define PIN_I2C_SDA      4
#define PIN_I2C_SCL      5
#define PIN_TOUCH        3      // TTP223 OUT
#define PIN_BUZZER       2      // piezo disc (100R series resistor recommended)
#define PIN_VIBRO        -1     // vibration motor module; -1 = not fitted
#define PIN_LED          8      // on-board LED, active LOW

// TTP223 modules ship in "momentary, active HIGH" mode. If your module is
// latched or active LOW, flip this instead of rewiring.
#define TOUCH_ACTIVE_HIGH    1

// ------------------------------- display ----------------------------------
#define OLED_WIDTH       128
#define OLED_HEIGHT      64
#define OLED_ADDR        0x3C
#define OLED_RESET       -1

// ------------------------------- timing -----------------------------------
#define LONG_PRESS_MS    700    // hold this long -> show text + play track 2
#define TOUCH_DEBOUNCE_MS 25
#define FRAME_MS         33     // ~30 fps animation tick
#define TEXT_MIN_MS      6000   // minimum time the text screen stays up
#define TEXT_PER_LINE_MS 900    // per extra line when scrolling long text

// -------------------------------- audio -----------------------------------
// 1 = piezo disc on PIN_BUZZER (default), 0 = DFPlayer Mini over UART.
// The Audio class hides the difference, so nothing else has to care.
#define AUDIO_DRIVER_PIEZO   1

#define AUDIO_VOLUME_DEFAULT 20   // 0..30; 0 = mute
#define TRACK_INTRO      1        // played once at boot
#define TRACK_LONGPRESS  2        // played with the long-press text screen
#define TRACK_TAP        3        // spare melody, not bound to a gesture

// --- piezo driver ---------------------------------------------------------
#define BUZZER_LEDC_CHANNEL  0
#define BUZZER_LEDC_BITS     10   // duty 0..1023; 512 = 50% = loudest
#define BUZZER_IDLE_FREQ     2000 // Hz, only used to seed ledcSetup

// --- DFPlayer driver (AUDIO_DRIVER_PIEZO == 0) ----------------------------
#define DFPLAYER_BAUD    9600
#define PIN_DF_TX        6        // C3 TX -> DFPlayer RX (1k series resistor!)
#define PIN_DF_RX        7        // DFPlayer TX -> C3 RX
#define PIN_DF_BUSY      -1       // LOW while playing; -1 = not wired
#define DFPLAYER_USE_MP3_FOLDER 1 // 1 = /mp3/0001.mp3, 0 = /0001.mp3

// -------------------------------- storage ---------------------------------
#define TEXT_PATH        "/text.txt"
#define PREFS_PATH       "/prefs.json"
#define TEXT_MAX_LEN     512    // characters accepted from the web panel

// ------------------------------- web UI -----------------------------------
#define HTTP_PORT        80
#define WS_PATH          "/ws"
#define AP_SSID          "DeskBuddy-Setup"
#define WIFI_CONNECT_MS  15000  // STA attempt before falling back to AP
