#ifndef CONFIG_H
#define CONFIG_H

// ============================================================================
// DISPLAY DRIVER SELECTION — uncomment exactly one
// ============================================================================
// #define DISPLAY_DRIVER_I2C   // Adafruit SSD1306 over I2C
#define DISPLAY_DRIVER_SPI   // U8g2 SSD1306 over hardware SPI

#if defined(DISPLAY_DRIVER_I2C) && defined(DISPLAY_DRIVER_SPI)
  #error "Only one display driver may be selected at a time."
#endif
#if !defined(DISPLAY_DRIVER_I2C) && !defined(DISPLAY_DRIVER_SPI)
  #error "A display driver must be selected in config.h."
#endif

// ============================================================================
// HARDWARE — PIN DEFINITIONS
// ============================================================================

// MCP2515 CAN controller — SPI
#define CAN_CS_PIN   10
#define CAN_INT_PIN  2

// Push button — short press cycles parameter, long press toggles display type
#define BUTTON_PIN   4

// SPI display pins — only used when DISPLAY_DRIVER_SPI is active
// Hardware SPI MOSI = pin 11, SCK = pin 13 (fixed on Nano, no define needed)
#define DISP_CS_PIN   9
#define DISP_DC_PIN   8
#define DISP_RST_PIN  7

// I2C display — SDA = A4, SCL = A5 on Nano (hardware fixed)
#define OLED_I2C_ADDRESS  0x3C

// ============================================================================
// DISPLAY GEOMETRY
// ============================================================================

#define SCREEN_WIDTH   128
#define SCREEN_HEIGHT  64
#define X_PADDING      8

// ============================================================================
// DISPLAY PERFORMANCE
// I2C tops out around 12fps at 400kHz — 80ms interval is comfortable.
// SPI can sustain 20fps easily — 50ms interval.
// ============================================================================
#ifdef DISPLAY_DRIVER_I2C
  #define DISPLAY_UPDATE_INTERVAL  80    // ms (~12fps)
  #define I2C_CLOCK_SPEED          400000L
#endif

#ifdef DISPLAY_DRIVER_SPI
  #define DISPLAY_UPDATE_INTERVAL  50    // ms (~20fps)
#endif

// ============================================================================
// BUTTON TIMING
// ============================================================================

#define BUTTON_DEBOUNCE_MS    50    // Ignore transitions shorter than this
#define BUTTON_LONG_PRESS_MS  500   // Hold duration that triggers a long press

// ============================================================================
// CAN BUS
// ============================================================================

#define CAN_SPEED              CAN_250KBPS   // Link G4+ bus speed
#define CAN_CLOCK              MCP_16MHz     // 16MHz crystal on most MCP2515 boards
#define MAX_CAN_INIT_ATTEMPTS  10
#define CAN_INIT_RETRY_DELAY   100
#define MAX_MESSAGES_PER_LOOP  10
#define CAN_DATA_TIMEOUT       3000

// ============================================================================
// CAN STREAM IDs (Link G4+ CAN, decimal — standard 11-bit)
//   1010 (0x3F2): rpm | mgp | ect | iat
//   1011 (0x3F3): batt voltage | wheel speed | oil pressure | lambda1
//   1012 (0x3F4): %ethanol | gear | oil temp | aux11 | knock | dig in 3
//   1013 (0x3F5): ap (main) | fuel pressure
// ============================================================================

#define CAN_ID_STREAM1   1010
#define CAN_ID_STREAM2   1011
#define CAN_ID_STREAM3   1012
#define CAN_ID_STREAM4   1013

// ============================================================================
// GAUGE DISPLAY RANGES
// ============================================================================

#define TEMP_GAUGE_MIN   0.0f
#define TEMP_GAUGE_MAX   120.0f
#define VOLT_GAUGE_MIN   0.0f
#define VOLT_GAUGE_MAX   20.0f

// Bar graph geometry (pixels)
#define BAR_WIDTH        6
#define BAR_HEIGHT       21
#define GAUGE_LENGTH     19

// ============================================================================
// SERIAL
// ============================================================================

#define SERIAL_BAUD_RATE  115200

// ============================================================================
// FEATURE FLAGS
// ============================================================================

#define ENABLE_SPLASH_SCREEN  false

#endif // CONFIG_H
