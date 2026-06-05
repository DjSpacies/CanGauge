// ============================================================================
// CanGauge — Arduino Nano USB-C
// Hardware : MCP2515 CAN (SPI), LM2596 PSU
// Display  : Configurable — set DISPLAY_DRIVER_I2C or DISPLAY_DRIVER_SPI
//            in config.h. No other file needs to change.
// Input    : Single push button
//              Short press (< BUTTON_LONG_PRESS_MS) — next parameter
//              Long press  (≥ BUTTON_LONG_PRESS_MS) — toggle bar / text
// Parameters: ECT, Oil Temp, Voltage
// ============================================================================

#include "config.h"
#include "display.h"
#include "mcp2515_can.h"

// ============================================================================
// CAN
// ============================================================================
mcp2515_can CAN(CAN_CS_PIN);

// ============================================================================
// PARAMETERS
// The active parameter and display type are tracked as two separate state
// variables rather than a flat mode enum, which maps naturally to the
// short press / long press input model.
// ============================================================================
enum Parameter : uint8_t {
    PARAM_ECT = 0,
    PARAM_OIL,
    PARAM_VOLT,
    PARAM_COUNT
};

enum DisplayType : uint8_t {
    DISP_BAR = 0,
    DISP_TEXT
};

static Parameter    currentParam = PARAM_ECT;
static DisplayType  currentDisp  = DISP_BAR;

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
static bool          displayDirty      = true;  // Force first draw

// ============================================================================
// BUTTON STATE MACHINE
//
// States:
//   IDLE        — button is not pressed
//   PRESSED     — button went low, timing has started, debounce not yet cleared
//   DEBOUNCED   — debounce period elapsed, press is confirmed, still held
//   FIRED_LONG  — long press action has already been dispatched, waiting for
//                 release so we do not also fire a short press on release
//
// Actions fire as follows:
//   Long press  — fires immediately when hold duration reaches BUTTON_LONG_PRESS_MS
//   Short press — fires on release, provided the long press threshold was not reached
//
// This gives the long press instant tactile feedback (the display type flips
// the moment you have held long enough) while the short press fires cleanly
// on release with no delay.
// ============================================================================
enum ButtonState : uint8_t {
    BTN_IDLE = 0,
    BTN_PRESSED,
    BTN_DEBOUNCED,
    BTN_FIRED_LONG
};

static ButtonState   btnState     = BTN_IDLE;
static unsigned long btnPressTime = 0;  // millis() when the press was confirmed

static void handleButton() {
    uint8_t      reading = digitalRead(BUTTON_PIN);  // LOW = pressed (INPUT_PULLUP)
    unsigned long now    = millis();

    switch (btnState) {

        case BTN_IDLE:
            if (reading == LOW) {
                btnState     = BTN_PRESSED;
                btnPressTime = now;
            }
            break;

        case BTN_PRESSED:
            if (reading == HIGH) {
                // Released before debounce elapsed — treat as noise, reset
                btnState = BTN_IDLE;
            } else if (now - btnPressTime >= BUTTON_DEBOUNCE_MS) {
                // Press confirmed — move to debounced state
                btnState     = BTN_DEBOUNCED;
                btnPressTime = now;  // Reset timer from confirmed press moment
            }
            break;

        case BTN_DEBOUNCED:
            if (now - btnPressTime >= BUTTON_LONG_PRESS_MS) {
                // ---- LONG PRESS ACTION ----
                // Toggle display type between bar graph and text
                currentDisp  = (currentDisp == DISP_BAR) ? DISP_TEXT : DISP_BAR;
                displayDirty = true;
                btnState     = BTN_FIRED_LONG;

                Serial.print(F("Long press — display: "));
                Serial.println(currentDisp == DISP_BAR ? F("Bar") : F("Text"));
            } else if (reading == HIGH) {
                // ---- SHORT PRESS ACTION ----
                // Released before long press threshold — advance to next parameter
                currentParam = (Parameter)((currentParam + 1) % PARAM_COUNT);
                displayDirty = true;
                btnState     = BTN_IDLE;

                Serial.print(F("Short press — param: "));
                switch (currentParam) {
                    case PARAM_ECT:  Serial.println(F("ECT"));     break;
                    case PARAM_OIL:  Serial.println(F("Oil"));     break;
                    case PARAM_VOLT: Serial.println(F("Voltage")); break;
                    default: break;
                }
            }
            break;

        case BTN_FIRED_LONG:
            // Long press has already fired — wait for release before resetting
            if (reading == HIGH) {
                btnState = BTN_IDLE;
            }
            break;
    }
}

// ============================================================================
// STALE DATA DETECTION
// Returns true if the value for the current parameter has not been updated
// within CAN_DATA_TIMEOUT milliseconds.
// ============================================================================
static bool isStale() {
    unsigned long now = millis();
    switch (currentParam) {
        case PARAM_ECT:  return (now - ectLastUpdate)  > CAN_DATA_TIMEOUT;
        case PARAM_OIL:  return (now - oilLastUpdate)  > CAN_DATA_TIMEOUT;
        case PARAM_VOLT: return (now - voltLastUpdate) > CAN_DATA_TIMEOUT;
        default:         return false;
    }
}

// ============================================================================
// CAN PROCESSING
// Drains up to MAX_MESSAGES_PER_LOOP frames per call.
// All three parameters are always updated regardless of what is displayed,
// so values are current the moment the user switches to them.
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
                    uint16_t raw    = (uint16_t)buf[6] | ((uint16_t)buf[7] << 8);
                    float    newVal = (float)raw - 50.0f;
                    if (newVal != ectValue) {
                        ectValue     = newVal;
                        // Only mark dirty if this is the parameter being shown
                        if (currentParam == PARAM_ECT) displayDirty = true;
                    }
                    ectLastUpdate = millis();
                }
                break;

            case CAN_MSG_OIL:
                if (len >= 4) {
                    uint16_t raw    = (uint16_t)buf[2] | ((uint16_t)buf[3] << 8);
                    float    newVal = (float)raw - 50.0f;
                    if (newVal != oilValue) {
                        oilValue     = newVal;
                        if (currentParam == PARAM_OIL) displayDirty = true;
                    }
                    oilLastUpdate = millis();
                }
                break;

            case CAN_MSG_VOLTAGE:
                if (len >= 6) {
                    uint16_t raw    = (uint16_t)buf[4] | ((uint16_t)buf[5] << 8);
                    float    newVal = raw / 100.0f;
                    if (newVal != voltValue) {
                        voltValue    = newVal;
                        if (currentParam == PARAM_VOLT) displayDirty = true;
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
// RENDER
// Dispatches to the correct display function based on current parameter and
// display type. The underlying library (I2C or SPI) is invisible to this code.
// ============================================================================
static void renderDisplay() {
    bool stale = isStale();

    switch (currentParam) {

        case PARAM_ECT:
            if (currentDisp == DISP_BAR) {
                displayBarGraph("ECT", ectValue,
                                TEMP_GAUGE_MIN, TEMP_GAUGE_MAX, "C", 0, stale);
            } else {
                displayTextScreen("ECT", ectValue, "C", 0, stale);
            }
            break;

        case PARAM_OIL:
            if (currentDisp == DISP_BAR) {
                displayBarGraph("Oil", oilValue,
                                TEMP_GAUGE_MIN, TEMP_GAUGE_MAX, "C", 0, stale);
            } else {
                displayTextScreen("Oil Temp", oilValue, "C", 0, stale);
            }
            break;

        case PARAM_VOLT:
            if (currentDisp == DISP_BAR) {
                displayBarGraph("Volt", voltValue,
                                VOLT_GAUGE_MIN, VOLT_GAUGE_MAX, "V", 1, stale);
            } else {
                displayTextScreen("Voltage", voltValue, "V", 1, stale);
            }
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
    // Note: no while(!Serial) — that blocks forever on a Nano without a
    // USB serial monitor open since it lacks native USB.

    pinMode(BUTTON_PIN, INPUT_PULLUP);

    displayInit();
    displaySplash();

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
    // 1. Drain the CAN receive buffer — non-blocking
    processCAN();

    // 2. Handle button input via state machine
    handleButton();

    // 3. Redraw the display if data changed, mode changed, or the refresh
    //    interval has elapsed (ensures stale detection updates on screen too)
    unsigned long now = millis();
    if (displayDirty || (now - lastDisplayUpdate) >= DISPLAY_UPDATE_INTERVAL) {
        renderDisplay();
        lastDisplayUpdate = now;
    }
}
