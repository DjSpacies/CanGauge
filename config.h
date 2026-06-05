#ifndef CONFIG_H
#define CONFIG_H

// ============================================================================
// DISPLAY DRIVER SELECTION — uncomment exactly one
// ============================================================================
#define DISPLAY_DRIVER_I2C   // Adafruit SSD1306 over I2C (Wire)
// #define DISPLAY_DRIVER_SPI   // U8g2 SSD1306 over hardware SPI

#if defined(DISPLAY_DRIVER_I2C) && defined(DISPLAY_DRIVER_SPI)
  #error "Only one display driver may be selected at a time."
#endif
#if !defined(DISPLAY_DRIVER_I2C) && !defined(DISPLAY_DRIVER_SPI)
  #error "A display driver must be selected in config.h."
#endif

// ============================================================================
// HARDWARE — PIN DEFINITIONS
// ============================================================================

// CAN (MCP2515) — SPI, shared bus
#define CAN_CS_PIN    10
#define CAN_INT_PIN   2

// Push button — cycles through display modes
#define BUTTON_PIN    4

// SPI display pins — only relevant when DISPLAY_DRIVER_SPI is selected
// These are ignored entirely when using I2C
#define DISP_CS_PIN   9
#define DISP_DC_PIN   8
#define DISP_RST_PIN  7

// I2C display — only relevant when DISPLAY_DRIVER_I2C is selected
// SDA = A4, SCL = A5 on Nano (hardware fixed, no define needed)
#define OLED_I2C_ADDRESS  0x3C

// ============================================================================
// DISPLAY GEOMETRY
// ============================================================================

#define SCREEN_WIDTH   128
#define SCREEN_HEIGHT  64
#define X_PADDING      8

// ============================================================================
// DISPLAY PERFORMANCE
// ============================================================================

// Target display refresh interval in milliseconds
// I2C is slower — 80ms (~12fps) is a comfortable ceiling at 400kHz
// SPI can push 50ms (~20fps) comfortably
#ifdef DISPLAY_DRIVER_I2C
  #define DISPLAY_UPDATE_INTERVAL  80
  #define I2C_CLOCK_SPEED          400000L  // 400kHz Fast Mode
#endif

#ifdef DISPLAY_DRIVER_SPI
  #define DISPLAY_UPDATE_INTERVAL  50
#endif

// ============================================================================
// CAN BUS CONFIGURATION
// ============================================================================

#define CAN_SPEED              CAN_125KBPS
#define CAN_CLOCK              MCP_16MHz      // Crystal on most MCP2515 boards
#define MAX_CAN_INIT_ATTEMPTS  10
#define CAN_INIT_RETRY_DELAY   100            // ms between retries
#define MAX_MESSAGES_PER_LOOP  10             // Max CAN frames drained per loop
#define CAN_DATA_TIMEOUT       3000           // ms before value shown as stale

// ============================================================================
// CAN MESSAGE IDs (first byte of your CAN packets)
// ============================================================================

#define CAN_MSG_ECT      2
#define CAN_MSG_VOLTAGE  3
#define CAN_MSG_OIL      8

// ============================================================================
// GAUGE DISPLAY RANGES
// ============================================================================

#define TEMP_GAUGE_MIN   0.0f
#define TEMP_GAUGE_MAX   120.0f
#define VOLT_GAUGE_MIN   0.0f
#define VOLT_GAUGE_MAX   20.0f

// Bar graph geometry
#define BAR_WIDTH        6
#define BAR_HEIGHT       21
#define GAUGE_LENGTH     19

// ============================================================================
// BUTTON DEBOUNCE
// ============================================================================

#define BUTTON_DEBOUNCE_MS  50

// ============================================================================
// SERIAL
// ============================================================================

#define SERIAL_BAUD_RATE  115200

// ============================================================================
// FEATURE FLAGS
// ============================================================================

#define ENABLE_SPLASH_SCREEN      true
#define ENABLE_STALE_DETECTION    true
#define CAN_DATA_TIMEOUT_MS       3000

#endif // CONFIG_H
