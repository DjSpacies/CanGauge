// ============================================================================
// CanGauge — Arduino Nano USB-C
// Hardware: MCP2515 CAN (SPI), LM2596 PSU
// Display:  Configurable — I2C SSD1306 (Adafruit) or SPI SSD1306 (U8g2)
//           Set DISPLAY_DRIVER_I2C or DISPLAY_DRIVER_SPI in config.h
// Input:    Single push button cycles through all display modes
// ============================================================================

#include "config.h"
#include "display.h"
#include "mcp2515_can.h"

// ============================================================================
// CAN
// ============================================================================
mcp2515_can CAN(CAN_CS_PIN);

// ============================================================================
// DISPLAY MODES
// Each mode is a unique combination of parameter + display style.
// The button cycles through them in order, wrapping back to the first.
// ============================================================================
enum DisplayMode : uint8_t {
    MODE_ECT_BAR = 0,
    MODE_ECT_TEXT,
    MODE_OIL_BAR,
    MODE_OIL_TEXT,
    MODE_VOLT_BAR,
    MODE_VOLT_TEXT,
    MODE_COUNT       // Always last — used for modulo wrap-around
};

static DisplayMode currentMode = MODE_ECT_BAR;

// ============================================================================
// LIVE DATA
// ============================================================================
static float ectValue  = 0.0f;
static float oilValue  = 0.0f;
static float voltValue = 0.0f;

static unsigned long ectLastUpdate  = 0;
static unsigned long oilLastUpdate  = 0;
static unsigned long voltLastUpdate = 0;

// ============================================================================
// DISPLAY STATE
// ============================================================================
static unsigned long lastDisplayUpdate = 0;
static bool          displayDirty      = true;  // Force draw on first loop

// ============================================================================
// BUTTON STATE
// ============================================================================
static uint8_t       buttonLastState   = HIGH;   // INPUT_PULLUP idles HIGH
static uint8_t       buttonStableState = HIGH;
static unsigned long buttonDebounceTime = 0;

// ============================================================================
// HELPERS
// ============================================================================

// Returns true if the data for the current mode has not been updated recently
static bool isStale(DisplayMode mode) {
    unsigned long now = millis();
    switch (mode) {
        case MODE_ECT_BAR:
        case MODE_ECT_TEXT:
            return (now - ectLastUpdate) > CAN_DATA_TIMEOUT;
        case MODE_OIL_BAR:
        case MODE_OIL_TEXT:
            return (now - oilLastUpdate) > CAN_DATA_TIMEOUT;
        case MODE_VOLT_BAR:
        case MODE_VOLT_TEXT:
            return (now - voltLastUpdate) > CAN_DATA_TIMEOUT;
        default:
            return false;
    }
}

// ============================================================================
// CAN PROCESSING
// Drains up to MAX_MESSAGES_PER_LOOP frames per call — non-blocking.
// All three parameters are always updated regardless of current display mode
// so that values are fresh when the user switches modes.
// ============================================================================
static void processCAN() {
    uint8_t msgCount = 0;
    uint8_t len      = 0;
    uint8_t buf[8];

    while (msgCount < MAX_MESSAGES_PER_LOOP && CAN_MSGAVAIL == CAN.checkReceive()) {
        CAN.readMsgBuf(&len, buf);
        msgCount++;

        if (len < 1) continue;

        switch (buf[0]) {

            case CAN_MSG_ECT:
                if (len >= 8) {
                    uint16_t raw = (uint16_t)buf[6] | ((uint16_t)buf[7] << 8);
                    float newVal = (float)raw - 50.0f;
                    if (newVal != ectValue) {
                        ectValue     = newVal;
                        displayDirty = true;
                    }
                    ectLastUpdate = millis();
                }
                break;

            case CAN_MSG_OIL:
                if (len >= 4) {
                    uint16_t raw = (uint16_t)buf[2] | ((uint16_t)buf[3] << 8);
                    float newVal = (float)raw - 50.0f;
                    if (newVal != oilValue) {
                        oilValue     = newVal;
                        displayDirty = true;
                    }
                    oilLastUpdate = millis();
                }
                break;

            case CAN_MSG_VOLTAGE:
                if (len >= 6) {
                    uint16_t raw = (uint16_t)buf[4] | ((uint16_t)buf[5] << 8);
                    float newVal = raw / 100.0f;
                    if (newVal != voltValue) {
                        voltValue    = newVal;
                        displayDirty = true;
                    }
                    voltLastUpdate = millis();
                }
                break;

            default:
                break;
        }
    }
}

// ============================================================================
// BUTTON HANDLING
// Clean debounce — fires once on the falling edge (press), not on release.
// ============================================================================
static void handleButton() {
    uint8_t reading = digitalRead(BUTTON_PIN);

    if (reading != buttonLastState) {
        buttonDebounceTime = millis();
    }

    if ((millis() - buttonDebounceTime) > BUTTON_DEBOUNCE_MS) {
        if (reading != buttonStableState) {
            buttonStableState = reading;
            if (buttonStableState == LOW) {
                // Confirmed press — advance to next mode
                currentMode  = (DisplayMode)((currentMode + 1) % MODE_COUNT);
                displayDirty = true;
                Serial.print(F("Mode: "));
                Serial.println(currentMode);
            }
        }
    }

    buttonLastState = reading;
}

// ============================================================================
// RENDER
// Dispatches to the correct display function for the active mode.
// The display implementation (I2C or SPI) is selected at compile time.
// ============================================================================
static void renderDisplay() {
    bool stale = isStale(currentMode);

    switch (currentMode) {
        case MODE_ECT_BAR:
            displayBarGraph("ECT", ectValue,
                            TEMP_GAUGE_MIN, TEMP_GAUGE_MAX, "C", 0, stale);
            break;
        case MODE_ECT_TEXT:
            displayTextScreen("ECT", ectValue, "C", 0, stale);
            break;
        case MODE_OIL_BAR:
            displayBarGraph("Oil", oilValue,
                            TEMP_GAUGE_MIN, TEMP_GAUGE_MAX, "C", 0, stale);
            break;
        case MODE_OIL_TEXT:
            displayTextScreen("Oil Temp", oilValue, "C", 0, stale);
            break;
        case MODE_VOLT_BAR:
            displayBarGraph("Volt", voltValue,
                            VOLT_GAUGE_MIN, VOLT_GAUGE_MAX, "V", 1, stale);
            break;
        case MODE_VOLT_TEXT:
            displayTextScreen("Voltage", voltValue, "V", 1, stale);
            break;
        default:
            break;
    }

    displayDirty = false;
}

// ============================================================================
// SETUP
// ============================================================================
void setup() {
    Serial.begin(SERIAL_BAUD_RATE);
    // No while(!Serial) — that blocks forever on Nano without a monitor open

    pinMode(BUTTON_PIN, INPUT_PULLUP);

    // Initialise display and show splash via the abstraction layer
    displayInit();
    displaySplash();

    // Initialise CAN with retries
    Serial.println(F("Initialising CAN..."));
    uint8_t attempts = 0;
    while (attempts < MAX_CAN_INIT_ATTEMPTS) {
        if (CAN_OK == CAN.begin(CAN_SPEED, CAN_CLOCK)) {
            Serial.println(F("CAN OK"));
            break;
        }
        Serial.println(F("CAN fail, retrying..."));
        delay(CAN_INIT_RETRY_DELAY);
        attempts++;
    }

    if (attempts >= MAX_CAN_INIT_ATTEMPTS) {
        Serial.println(F("CAN init failed after max attempts — continuing"));
    }

    lastDisplayUpdate = millis();
}

// ============================================================================
// LOOP
// ============================================================================
void loop() {
    // 1. Drain CAN receive buffer — non-blocking
    processCAN();

    // 2. Check button input
    handleButton();

    // 3. Redraw display if data changed or the refresh interval has elapsed
    unsigned long now = millis();
    if (displayDirty || (now - lastDisplayUpdate) >= DISPLAY_UPDATE_INTERVAL) {
        renderDisplay();
        lastDisplayUpdate = now;
    }
}
