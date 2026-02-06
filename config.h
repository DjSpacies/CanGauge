#ifndef CONFIG_H
#define CONFIG_H

// ============================================================================
// HARDWARE CONFIGURATION
// ============================================================================

// Board Selection - uncomment the board you're using
#define BOARD_NANO
// #define BOARD_MEGA
// #define BOARD_UNO

// Pin Definitions
#ifdef BOARD_NANO
  #define SPI_CS_PIN 10
  #define CAN_INT_PIN 7
  #define ENCODER_CLK_PIN 2
  #define ENCODER_DT_PIN 3
  #define ENCODER_BTN_PIN 4
  // I2C pins are fixed on Nano: A4 (SDA), A5 (SCL)
#endif

#ifdef BOARD_MEGA
  #define SPI_CS_PIN 10
  #define CAN_INT_PIN 2
  #define ENCODER_CLK_PIN 3
  #define ENCODER_DT_PIN 4
  #define ENCODER_BTN_PIN 5
  // I2C pins are fixed on Mega: 20 (SDA), 21 (SCL)
#endif

#ifdef BOARD_UNO
  #define SPI_CS_PIN 10
  #define CAN_INT_PIN 7
  #define ENCODER_CLK_PIN 2
  #define ENCODER_DT_PIN 3
  #define ENCODER_BTN_PIN 4
  // I2C pins are fixed on Uno: A4 (SDA), A5 (SCL)
#endif

// ============================================================================
// DISPLAY CONFIGURATION
// ============================================================================

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDRESS 0x3C
#define OLED_RESET -1
#define X_PADDING 8

// Display update settings
#define DISPLAY_UPDATE_INTERVAL 100  // Milliseconds between display updates
#define DISPLAY_THROTTLE_ENABLED true

// Bar graph dimensions
#define BAR_WIDTH 6
#define BAR_HEIGHT 21
#define GAUGE_LENGTH 19

// ============================================================================
// CAN BUS CONFIGURATION
// ============================================================================

// CAN Bus Speed - uncomment the one you're using
#define CAN_SPEED CAN_125KBPS
// #define CAN_SPEED CAN_250KBPS
// #define CAN_SPEED CAN_500KBPS
// #define CAN_SPEED CAN_1000KBPS

// CAN initialization settings
#define MAX_CAN_INIT_ATTEMPTS 10
#define CAN_INIT_RETRY_DELAY 100  // Milliseconds

// CAN message processing
#define MAX_MESSAGES_PER_LOOP 5   // How many CAN messages to process per loop
#define CAN_DATA_TIMEOUT 2000      // Milliseconds before data is considered stale

// ============================================================================
// CAN PACKET DEFINITIONS
// ============================================================================

// CAN Message IDs (first byte of CAN packet)
#define CAN_MSG_ID_ECT 2
#define CAN_MSG_ID_VOLTAGE 3
#define CAN_MSG_ID_OIL_TEMP 8

// ECT (Engine Coolant Temperature) Configuration
struct ECTConfig {
  static const uint8_t messageID = CAN_MSG_ID_ECT;
  static const uint8_t minDataLength = 8;      // Minimum packet length required
  static const uint8_t startBit = 55;          // Bit position in packet
  static const uint8_t bytePosition = 6;       // Calculated: startBit / 8
  static const uint8_t bitLength = 16;         // Number of bits
  static const int16_t offset = 50;            // Value offset
  static const float scale = 1.0;              // Scaling factor
  static const float minValid = -40.0;         // Minimum valid value
  static const float maxValid = 200.0;         // Maximum valid value
  static const char* label = "ECT";
  static const char* units = "C";
  static const char* longLabel = "ECT: ";
};

// Oil Temperature Configuration
struct OilTempConfig {
  static const uint8_t messageID = CAN_MSG_ID_OIL_TEMP;
  static const uint8_t minDataLength = 4;
  static const uint8_t startBit = 23;
  static const uint8_t bytePosition = 2;       // Calculated: startBit / 8
  static const uint8_t bitLength = 16;
  static const int16_t offset = 50;
  static const float scale = 1.0;
  static const float minValid = -40.0;
  static const float maxValid = 200.0;
  static const char* label = "Oil";
  static const char* longLabel = "Oil Temp: ";
  static const char* units = "C";
};

// Voltage Configuration
struct VoltageConfig {
  static const uint8_t messageID = CAN_MSG_ID_VOLTAGE;
  static const uint8_t minDataLength = 6;
  static const uint8_t startBit = 39;
  static const uint8_t bytePosition = 4;       // Calculated: startBit / 8
  static const uint8_t bitLength = 16;
  static const int16_t offset = 0;
  static const float scale = 0.01;             // Value is divided by 100
  static const float minValid = 0.0;
  static const float maxValid = 20.0;
  static const char* label = "Volt";
  static const char* longLabel = "Voltage: ";
  static const char* units = "V";
  static const uint8_t decimalPlaces = 1;      // How many decimal places to display
};

// ============================================================================
// ADVANCED CAN PACKET PARSING
// ============================================================================

// If your CAN packets use different endianness, change this
#define CAN_LITTLE_ENDIAN true
// #define CAN_BIG_ENDIAN true

// If your CAN packets are signed values
#define ECT_IS_SIGNED false
#define OIL_TEMP_IS_SIGNED false
#define VOLTAGE_IS_SIGNED false

// ============================================================================
// DISPLAY THRESHOLDS
// ============================================================================

// Minimum change required to trigger display update
#define TEMP_CHANGE_THRESHOLD 1.0    // Degrees
#define VOLTAGE_CHANGE_THRESHOLD 0.1 // Volts

// Gauge display ranges (for bar graph mode)
#define TEMP_GAUGE_MAX 120.0         // Maximum temperature for gauge
#define VOLTAGE_GAUGE_MAX 20.0       // Maximum voltage for gauge

// ============================================================================
// SERIAL CONFIGURATION
// ============================================================================

#define SERIAL_BAUD_RATE 115200
#define SERIAL_ENABLED true          // Set to false to disable all serial output
#define SERIAL_DEBUG false           // Set to true for verbose debugging

// ============================================================================
// INPUT CONFIGURATION
// ============================================================================

#define ENCODER_DEBOUNCE_DELAY 50    // Milliseconds
#define BUTTON_DEBOUNCE_DELAY 50     // Milliseconds

// ============================================================================
// I2C CONFIGURATION
// ============================================================================

#define I2C_CLOCK_SPEED 400000L      // 400kHz (Fast Mode)
// #define I2C_CLOCK_SPEED 100000L   // 100kHz (Standard Mode) - use if you have issues

// ============================================================================
// FEATURE FLAGS
// ============================================================================

#define ENABLE_SPLASH_SCREEN true
#define ENABLE_STALE_DATA_DETECTION true
#define ENABLE_CAN_INTERRUPT false    // Set to true to use interrupt-driven CAN
#define ENABLE_VALUE_VALIDATION true  // Validate values are within min/max range

#endif // CONFIG_H