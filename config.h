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

// Second push button (found via pin scan) — long press opens the shift light menu
#define BUTTON2_PIN  3

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
#define CAN_CLOCK              MCP_8MHz     // 8MHz crystal verified
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

// AP (accelerator position) — TEMPORARY latency-test parameter.
// Comment out the line below to strip AP entirely from the final build.
#define ENABLE_AP_PARAM
#define AP_GAUGE_MIN   0.0f
#define AP_GAUGE_MAX   100.0f

// ============================================================================
// SHIFT LIGHT
// A solid bar sweeps left to right over the last SHIFT_RAMP_RPM below the shift point, then
// the whole screen flashes "SHIFT" (text/background inverting). Overrides
// whatever screen is showing. Comment out ENABLE_SHIFT_LIGHT to strip it.
// ============================================================================
#define ENABLE_SHIFT_LIGHT

// BENCH TESTING: drive the shift light from AP (0-100%) instead of RPM,
// mapped linearly to 0..SHIFT_TEST_RPM_MAX. Comment out to use real RPM.
// Requires ENABLE_AP_PARAM.
// #define SHIFT_TEST_USE_AP
#define SHIFT_TEST_RPM_MAX         8500   // Must exceed SHIFT_MENU_MAX_RPM or high shift points can't be reached with AP

#define SHIFT_RPM_DEFAULT          6000   // Shift point
#define SHIFT_RAMP_RPM             500    // Edge bars ramp across this window
#define SHIFT_HYSTERESIS_RPM       50     // Flash stops this far below shift point
#define SHIFT_MIN_RPM              2000   // Shift light disabled below this
#define SHIFT_FLASH_HALF_PERIOD_MS 60     // ~8Hz flash
#define SHIFT_DATA_TIMEOUT         500    // ms without input -> shift light off

// Menu: shift point is adjustable in steps between these limits (wraps around
// at either end) and saved to EEPROM.
#define SHIFT_MENU_MIN_RPM         6000
#define SHIFT_MENU_MAX_RPM         8000
#define SHIFT_MENU_STEP_RPM        100
#define SHIFT_EEPROM_ADDR          0      // Uses 6 bytes: shift record (4) + boost unit pair (2)
#define VIEW_EEPROM_ADDR           6      // Last page / display type: [packed][~packed]

#if defined(ENABLE_SHIFT_LIGHT) && defined(SHIFT_TEST_USE_AP) && !defined(ENABLE_AP_PARAM)
  #error "SHIFT_TEST_USE_AP requires ENABLE_AP_PARAM."
#endif

// ============================================================================
// BOOST / VACUUM (MGP, Link stream 1 bytes 2-3, 16-bit, 1 kPa/unit)
// ECU display offset is 100 kPa: CAN raw = gauge kPa + 100, so raw 0 = full
// vacuum (-100 kPa) and raw 100 = atmospheric (0 kPa gauge).
//
// Bar is zero-centred with zero ~1/3 from the left: 6 vacuum segments and 11
// boost segments of equal size, using the same segment grid as the other
// bars. 100/6 = 16.7 kPa per segment, so the bar spans -100 .. +183 kPa
// (about -29.5 inHg .. +26.6 psi).
// ============================================================================
#define BOOST_ECU_OFFSET_KPA       100
#define BOOST_VAC_SEGMENTS         6
#define BOOST_BOOST_SEGMENTS       11
#define BOOST_KPA_PER_SEGMENT      (100.0f / 6.0f)

// ============================================================================
// MIN / MAX / AVG HOLD
// Button B short press cycles the current screen: live -> MAX -> MIN -> AVG.
// Stats are sampled once per STATS_SAMPLE_MS while data is fresh and reset at
// power-up. The view returns to live after STATS_VIEW_TIMEOUT_MS of no button
// presses so a held value is never mistaken for a live one (0 = never).
// ============================================================================
#define ENABLE_STATS
#define STATS_SAMPLE_MS            1000
#define STATS_VIEW_TIMEOUT_MS      10000

// ============================================================================
// SERIAL
// ============================================================================

#define SERIAL_BAUD_RATE  115200

// ============================================================================
// FEATURE FLAGS
// ============================================================================

#define ENABLE_SPLASH_SCREEN  false

#endif // CONFIG_H
