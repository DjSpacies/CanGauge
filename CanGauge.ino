// ============================================================================
// CanGauge — Arduino Nano USB-C
// Hardware : MCP2515 CAN (SPI), LM2596 PSU
// Display  : Configurable — set DISPLAY_DRIVER_I2C or DISPLAY_DRIVER_SPI
//            in config.h. No other file needs to change.
// Input    : Two push buttons (A, B) — see BUTTONS section below
//              A short — next parameter, A long — toggle bar / text
//              B long  — shift point menu (saved to EEPROM)
// Shift light: overrides every screen, see SHIFT LIGHT section
// Parameters: ECT, Oil Temp, Voltage
// CAN      : Link G4+ — 250kbps, streams on IDs 1010-1013, MSB first
// ============================================================================

#include "config.h"
#include "display.h"
#include "mcp2515_can.h"
#include <EEPROM.h>

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
    PARAM_BOOST,
#ifdef ENABLE_AP_PARAM
    PARAM_AP,
#endif
    PARAM_COUNT
};

enum DisplayType : uint8_t {
    DISP_BAR = 0,
    DISP_TEXT
};

// Everything the render code needs to know about a Parameter (see paramCfg()).
struct ParamCfg {
    const char* barLabel;
    const char* textLabel;
    float       gmin;
    float       gmax;
    const char* unit;
    uint8_t     decimals;
};

// Boost / vacuum display unit. psi is used for boost and inHg for vacuum.
enum BoostUnit : uint8_t { UNIT_KPA = 0, UNIT_BAR, UNIT_PSI, UNIT_COUNT };
static uint8_t boostUnit = UNIT_PSI;

static Parameter    currentParam = PARAM_ECT;
static DisplayType  currentDisp  = DISP_BAR;

// ============================================================================
// LIVE DATA
// ============================================================================
static float ectValue  = 0.0f;
static float oilValue  = 0.0f;
static float voltValue = 0.0f;
static float boostKpa  = 0.0f;   // Gauge pressure, kPa (negative = vacuum)

#ifdef ENABLE_AP_PARAM
static float apValue   = 0.0f;
static unsigned long apLastUpdate = 0;
#endif

static unsigned long ectLastUpdate  = 0;
static unsigned long oilLastUpdate  = 0;
static unsigned long voltLastUpdate = 0;
static unsigned long boostLastUpdate = 0;

#ifdef ENABLE_SHIFT_LIGHT
static uint16_t      rpmValue      = 0;
static unsigned long rpmLastUpdate = 0;

static uint16_t shiftRpm      = SHIFT_RPM_DEFAULT;  // Shift point (menu will edit this)
static bool     shiftFlashing = false;              // At/above shift point
static bool     shiftPhase    = false;              // Current flash phase (inverted?)
static uint8_t  shiftBarPx    = 0;                  // Ramp bar width (px) during ramp
#endif

// ============================================================================
// MIN / MAX / AVG STATS
// One entry per Parameter. avg is a running mean over n samples.
// ============================================================================
#ifdef ENABLE_STATS
enum StatView : uint8_t { VIEW_LIVE = 0, VIEW_MAX, VIEW_MIN, VIEW_AVG, VIEW_COUNT };

struct Stat {
    float    mn;
    float    mx;
    float    avg;
    uint32_t n;
};

static Stat          stats[PARAM_COUNT];
static StatView      statView        = VIEW_LIVE;
static unsigned long statViewSince   = 0;  // Last button activity while in a stats view
static unsigned long lastStatSample  = 0;
#endif

// ============================================================================
// DISPLAY STATE
// ============================================================================
static unsigned long lastDisplayUpdate = 0;
static bool          displayDirty      = true;  // Force first draw

// ============================================================================
// BUTTONS
// Two buttons — A (BUTTON_PIN) and B (BUTTON2_PIN) — wired to GND with
// INPUT_PULLUP (LOW = pressed). Each has its own small state machine
// (pollButton) that returns at most one event per call:
//
//   BTN_EVT_LONG  — fires immediately once the press has been held for
//                   BUTTON_LONG_PRESS_MS (instant tactile feedback)
//   BTN_EVT_SHORT — fires on release, provided the long press did not fire
//
// States:
//   IDLE        — not pressed
//   PRESSED     — went low, debounce not yet cleared
//   DEBOUNCED   — press confirmed, still held, timing the long press
//   FIRED_LONG  — long press dispatched, waiting for release so no short
//                 press is also fired
//
// Outside the menu:
//   A short = next parameter, A long = toggle bar/text,
//   B long  = open settings menu (shift point, boost units) (B short is reserved for min/max hold)
// In the menu:
//   A short = shift point down, B short = shift point up (wraps at the ends),
//   B long  = next item / save and exit on the last item,
//   A long  = cancel and exit
// ============================================================================
enum ButtonState : uint8_t {
    BTN_IDLE = 0,
    BTN_PRESSED,
    BTN_DEBOUNCED,
    BTN_FIRED_LONG
};

enum ButtonEvent : uint8_t {
    BTN_EVT_NONE = 0,
    BTN_EVT_SHORT,
    BTN_EVT_LONG
};

struct Button {
    uint8_t       pin;
    ButtonState   state;
    unsigned long pressTime;  // millis() at press / debounce confirmation
};

static Button btnA = { BUTTON_PIN,  BTN_IDLE, 0 };
static Button btnB = { BUTTON2_PIN, BTN_IDLE, 0 };

static ButtonEvent pollButton(Button &b) {
    bool          released = (digitalRead(b.pin) == HIGH);
    unsigned long now      = millis();

    switch (b.state) {
        case BTN_IDLE:
            if (!released) {
                b.state     = BTN_PRESSED;
                b.pressTime = now;
            }
            break;

        case BTN_PRESSED:
            if (released) {
                b.state = BTN_IDLE;                 // Noise — released too soon
            } else if (now - b.pressTime >= BUTTON_DEBOUNCE_MS) {
                b.state     = BTN_DEBOUNCED;        // Press confirmed
                b.pressTime = now;
            }
            break;

        case BTN_DEBOUNCED:
            if (now - b.pressTime >= BUTTON_LONG_PRESS_MS) {
                b.state = BTN_FIRED_LONG;
                return BTN_EVT_LONG;
            } else if (released) {
                b.state = BTN_IDLE;
                return BTN_EVT_SHORT;
            }
            break;

        case BTN_FIRED_LONG:
            if (released) b.state = BTN_IDLE;
            break;
    }
    return BTN_EVT_NONE;
}

#ifdef ENABLE_SHIFT_LIGHT
// ----------------------------------------------------------------------------
// SHIFT POINT MENU + EEPROM
// EEPROM layout at SHIFT_EEPROM_ADDR: [magic][rpm lo][rpm hi][checksum].
// A blank / corrupt / out-of-range record falls back to SHIFT_RPM_DEFAULT.
// Writes use update() so unchanged bytes cost no EEPROM wear.
// ----------------------------------------------------------------------------
#define SHIFT_EEPROM_MAGIC 0xC6

static bool     menuActive = false;
static uint8_t  menuItem   = 0;      // 0 = shift point, 1 = boost units
static uint16_t menuRpm    = 0;      // Values being edited (committed on save)
static uint8_t  menuUnit   = 0;

static uint8_t shiftChecksum(uint16_t v) {
    return SHIFT_EEPROM_MAGIC ^ (v & 0xFF) ^ (v >> 8) ^ 0x5A;
}

static void loadSettings() {
    uint16_t v = EEPROM.read(SHIFT_EEPROM_ADDR + 1) |
                ((uint16_t)EEPROM.read(SHIFT_EEPROM_ADDR + 2) << 8);
    bool ok = EEPROM.read(SHIFT_EEPROM_ADDR) == SHIFT_EEPROM_MAGIC &&
              EEPROM.read(SHIFT_EEPROM_ADDR + 3) == shiftChecksum(v) &&
              v >= SHIFT_MENU_MIN_RPM && v <= SHIFT_MENU_MAX_RPM;
    shiftRpm = ok ? v : SHIFT_RPM_DEFAULT;
    Serial.print(F("Shift point: "));
    Serial.print(shiftRpm);
    Serial.println(ok ? F(" (saved)") : F(" (default)"));

    // Boost unit: [unit][~unit] pair right after the shift record.
    uint8_t u = EEPROM.read(SHIFT_EEPROM_ADDR + 4);
    bool uok  = u < UNIT_COUNT && EEPROM.read(SHIFT_EEPROM_ADDR + 5) == (uint8_t)~u;
    if (uok) boostUnit = u;
    Serial.print(F("Boost unit: "));
    Serial.print(boostUnit);
    Serial.println(uok ? F(" (saved)") : F(" (default)"));
}

static void saveSettings(uint16_t rpm, uint8_t unit) {
    EEPROM.update(SHIFT_EEPROM_ADDR,     SHIFT_EEPROM_MAGIC);
    EEPROM.update(SHIFT_EEPROM_ADDR + 1, rpm & 0xFF);
    EEPROM.update(SHIFT_EEPROM_ADDR + 2, rpm >> 8);
    EEPROM.update(SHIFT_EEPROM_ADDR + 3, shiftChecksum(rpm));
    EEPROM.update(SHIFT_EEPROM_ADDR + 4, unit);
    EEPROM.update(SHIFT_EEPROM_ADDR + 5, (uint8_t)~unit);
    shiftRpm  = rpm;
    boostUnit = unit;
}

// Menu items: 0 = shift point, 1 = boost units.
//   A short / B short - adjust the current item (A down/prev, B up/next)
//   B long            - next item; on the last item saves everything and exits
//   A long            - cancel (discard all edits) and exit
static void handleMenu(ButtonEvent a, ButtonEvent b) {
    if (a == BTN_EVT_SHORT || b == BTN_EVT_SHORT) {
        bool up = (b == BTN_EVT_SHORT);
        if (menuItem == 0) {
            if (up) {
                menuRpm = (menuRpm + SHIFT_MENU_STEP_RPM > SHIFT_MENU_MAX_RPM)
                              ? SHIFT_MENU_MIN_RPM : menuRpm + SHIFT_MENU_STEP_RPM;
            } else {
                menuRpm = (menuRpm < SHIFT_MENU_MIN_RPM + SHIFT_MENU_STEP_RPM)
                              ? SHIFT_MENU_MAX_RPM : menuRpm - SHIFT_MENU_STEP_RPM;
            }
        } else {
            menuUnit = up ? (menuUnit + 1) % UNIT_COUNT
                          : (menuUnit + UNIT_COUNT - 1) % UNIT_COUNT;
        }
        displayDirty = true;
    } else if (b == BTN_EVT_LONG) {
        if (menuItem == 0) {
            menuItem = 1;
        } else {
            saveSettings(menuRpm, menuUnit);
            menuActive = false;
            Serial.print(F("Menu: saved shift point "));
            Serial.print(shiftRpm);
            Serial.print(F(", boost unit "));
            Serial.println(boostUnit);
        }
        displayDirty = true;
    } else if (a == BTN_EVT_LONG) {
        menuActive   = false;
        displayDirty = true;
        Serial.println(F("Menu: cancelled"));
    }
}
#endif

// ----------------------------------------------------------------------------
// LAST VIEW PERSISTENCE
// The current parameter and bar/text choice are saved to EEPROM whenever they
// change and restored at boot. Stats views and the menu are never saved.
// Stored as [packed][~packed] where packed = param | (disp << 4); anything
// invalid falls back to the defaults. update() skips unchanged bytes.
// ----------------------------------------------------------------------------
static void saveView() {
    uint8_t packed = (uint8_t)currentParam | ((uint8_t)currentDisp << 4);
    EEPROM.update(VIEW_EEPROM_ADDR,     packed);
    EEPROM.update(VIEW_EEPROM_ADDR + 1, (uint8_t)~packed);
}

static void loadView() {
    uint8_t packed = EEPROM.read(VIEW_EEPROM_ADDR);
    uint8_t param  = packed & 0x0F;
    uint8_t disp   = packed >> 4;
    if (EEPROM.read(VIEW_EEPROM_ADDR + 1) == (uint8_t)~packed &&
        param < PARAM_COUNT && disp <= DISP_TEXT) {
        currentParam = (Parameter)param;
        currentDisp  = (DisplayType)disp;
    }
}

static void handleButtons() {
    ButtonEvent a = pollButton(btnA);
    ButtonEvent b = pollButton(btnB);

#ifdef ENABLE_SHIFT_LIGHT
    if (menuActive) {
        handleMenu(a, b);
        return;
    }
    if (b == BTN_EVT_LONG) {
        menuActive   = true;
        menuItem     = 0;
        menuRpm      = shiftRpm;
        menuUnit     = boostUnit;
        displayDirty = true;
        Serial.println(F("Menu: open"));
        return;
    }
#endif

#ifdef ENABLE_STATS
    if (b == BTN_EVT_SHORT) {
        statView      = (StatView)((statView + 1) % VIEW_COUNT);
        statViewSince = millis();
        displayDirty  = true;
        Serial.print(F("Stats view: "));
        Serial.println((uint8_t)statView);
        return;
    }
    if (a != BTN_EVT_NONE) statViewSince = millis();  // Keep stats view alive
#endif

    if (a == BTN_EVT_LONG) {
        // Toggle display type between bar graph and text
        currentDisp  = (currentDisp == DISP_BAR) ? DISP_TEXT : DISP_BAR;
        displayDirty = true;
        saveView();
        Serial.print(F("Long press — display: "));
        Serial.println(currentDisp == DISP_BAR ? F("Bar") : F("Text"));
    } else if (a == BTN_EVT_SHORT) {
        // Advance to next parameter
        currentParam = (Parameter)((currentParam + 1) % PARAM_COUNT);
        displayDirty = true;
        saveView();
        Serial.print(F("Short press — param: "));
        switch (currentParam) {
            case PARAM_ECT:  Serial.println(F("ECT"));     break;
            case PARAM_OIL:  Serial.println(F("Oil"));     break;
            case PARAM_VOLT: Serial.println(F("Voltage")); break;
            #ifdef ENABLE_AP_PARAM
                case PARAM_AP:   Serial.println(F("AP"));      break;
            #endif
            default: break;
        }
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
        case PARAM_BOOST: return (now - boostLastUpdate) > CAN_DATA_TIMEOUT;
        default:         return false;
    }
}

// ============================================================================
// CAN PROCESSING
// Drains up to MAX_MESSAGES_PER_LOOP frames per call.
//
// Link G4+ CAN streams (250kbps, MSB first / big-endian):
//   ID 1010 (Stream 1): rpm | mgp | ECT | iat        -> ECT at bytes 4,5
//   ID 1011 (Stream 2): batt V | wheel | oil P | l1  -> Voltage at bytes 0,1
//   ID 1012 (Stream 3): eth | gear | OIL TEMP | ...  -> Oil temp at byte 2 (8-bit)
//   ID 1013 (Stream 4): ap (main) | fuel pressure    -> (unused for now)
//
// Frames are identified by arbitration ID via CAN.getCanId(). All tracked
// parameters are always updated regardless of what is displayed, so values
// are current the moment the user switches to them.
// ============================================================================
static void processCAN() {
    uint8_t msgCount = 0;
    uint8_t len      = 0;
    uint8_t buf[8];

    while (msgCount < MAX_MESSAGES_PER_LOOP && CAN_MSGAVAIL == CAN.checkReceive()) {
        CAN.readMsgBuf(&len, buf);
        unsigned long canId = CAN.getCanId();
        msgCount++;

        switch (canId) {

            case CAN_ID_STREAM1:                       // RPM @ bytes 0,1 | ECT @ bytes 4,5
                #ifdef ENABLE_SHIFT_LIGHT
                if (len >= 2) {
                    rpmValue      = ((uint16_t)buf[0] << 8) | buf[1];  // 1 rpm/unit
                    rpmLastUpdate = millis();
                }
                #endif
                if (len >= 4) {                        // MGP @ bytes 2,3 (raw = kPa + offset)
                    uint16_t raw    = ((uint16_t)buf[2] << 8) | buf[3];
                    float    newVal = (float)raw - (float)BOOST_ECU_OFFSET_KPA;
                    if (newVal != boostKpa) {
                        boostKpa = newVal;
                        if (currentParam == PARAM_BOOST) displayDirty = true;
                    }
                    boostLastUpdate = millis();
                }
                if (len >= 6) {
                    uint16_t raw    = ((uint16_t)buf[4] << 8) | buf[5];
                    float    newVal = (float)raw - 50.0f;
                    if (newVal != ectValue) {
                        ectValue = newVal;
                        if (currentParam == PARAM_ECT) displayDirty = true;
                    }
                    ectLastUpdate = millis();
                }
                break;

            case CAN_ID_STREAM2:                       // Voltage @ bits 0-15 (bytes 0,1)
                if (len >= 2) {
                    uint16_t raw    = ((uint16_t)buf[0] << 8) | buf[1];
                    float    newVal = raw / 100.0f;
                    if (newVal != voltValue) {
                        voltValue = newVal;
                        if (currentParam == PARAM_VOLT) displayDirty = true;
                    }
                    voltLastUpdate = millis();
                }
                break;

            case CAN_ID_STREAM3:                       // Oil temp @ bits 16-23 (byte 2, 8-bit)
                if (len >= 3) {
                    uint16_t raw    = buf[2];
                    float    newVal = (float)raw - 50.0f;
                    if (newVal != oilValue) {
                        oilValue = newVal;
                        if (currentParam == PARAM_OIL) displayDirty = true;
                    }
                    oilLastUpdate = millis();
                }
                break;

            #ifdef ENABLE_AP_PARAM
                        case CAN_ID_STREAM4:                   // AP @ bits 0-15 (bytes 0,1)
                            if (len >= 2) {
                                uint16_t raw    = ((uint16_t)buf[0] << 8) | buf[1];
                                float    newVal = raw / 10.0f;     // 0.1% resolution
                                if (newVal != apValue) {
                                    apValue = newVal;
                                    if (currentParam == PARAM_AP) displayDirty = true;
                                }
                                apLastUpdate = millis();
                            }
                            break;
            #endif



            default:
                break;
        }
    }
}

// ============================================================================
// SHIFT LIGHT
// Input is real RPM, or AP mapped to a fake RPM when SHIFT_TEST_USE_AP is set.
// Returns false if the input is stale.
// ============================================================================
#ifdef ENABLE_SHIFT_LIGHT
static bool getShiftInput(uint16_t &rpm) {
    unsigned long now = millis();
#ifdef SHIFT_TEST_USE_AP
    if (now - apLastUpdate > SHIFT_DATA_TIMEOUT) return false;
    float ap = constrain(apValue, 0.0f, 100.0f);
    rpm = (uint16_t)(ap * (SHIFT_TEST_RPM_MAX / 100.0f));
#else
    if (now - rpmLastUpdate > SHIFT_DATA_TIMEOUT) return false;
    rpm = rpmValue;
#endif
    return true;
}

// Ramp: edge bars grow from 0 to full height across SHIFT_RAMP_RPM below the
// shift point. At/above the shift point: flashing. Off if the input is stale
// or below SHIFT_MIN_RPM. Flash holds until RPM drops SHIFT_HYSTERESIS_RPM
// below the shift point so it doesn't flicker at the threshold.
static void updateShiftLight() {
    uint16_t rpm    = 0;
    bool     active = getShiftInput(rpm) && rpm >= SHIFT_MIN_RPM;
    bool     flash  = shiftFlashing;
    uint8_t  barPx  = 0;

    if (!active) {
        flash = false;
    } else if (rpm >= shiftRpm) {
        flash = true;
    } else if (flash && (rpm + SHIFT_HYSTERESIS_RPM) < shiftRpm) {
        flash = false;
    }

    if (active && !flash) {
        uint16_t start = (shiftRpm > SHIFT_RAMP_RPM) ? (shiftRpm - SHIFT_RAMP_RPM) : 0;
        if (rpm > start) {
            uint32_t px = ((uint32_t)(rpm - start) * SCREEN_WIDTH) / SHIFT_RAMP_RPM;
            barPx = (px > SCREEN_WIDTH) ? SCREEN_WIDTH : (uint8_t)px;
        }
    }

    bool phase = flash && (((millis() / SHIFT_FLASH_HALF_PERIOD_MS) & 1) != 0);

    if (flash != shiftFlashing) {
        Serial.println(flash ? F("SHIFT!") : F("Shift off"));
    }
    if (flash != shiftFlashing || phase != shiftPhase || barPx != shiftBarPx) {
        displayDirty = true;
    }
    shiftFlashing = flash;
    shiftPhase    = phase;
    shiftBarPx    = barPx;
}
#endif

// ============================================================================
// BOOST UNIT CONVERSION
// kPa -> display value, unit string and decimals for the selected unit.
// psi/inHg mode shows boost in psi and vacuum in inHg (negative).
// ============================================================================
static void boostConvert(float kpa, float &out, const char* &unit, uint8_t &dec) {
    switch (boostUnit) {
        case UNIT_KPA: out = kpa;           unit = "kPa"; dec = 0; break;
        case UNIT_BAR: out = kpa / 100.0f;  unit = "bar"; dec = 2; break;
        default:
            if (kpa >= 0) { out = kpa * 0.145038f; unit = "psi";  dec = 1; }
            else          { out = kpa * 0.295300f; unit = "inHg"; dec = 1; }
            break;
    }
}

// ============================================================================
// PARAMETER TABLE
// Everything the render and stats code needs to know about each Parameter.
// ============================================================================
static ParamCfg paramCfg(Parameter p) {
    switch (p) {
        case PARAM_ECT:  return { "ECT",  "ECT",      TEMP_GAUGE_MIN, TEMP_GAUGE_MAX, "C", 0 };
        case PARAM_OIL:  return { "Oil",  "Oil Temp", TEMP_GAUGE_MIN, TEMP_GAUGE_MAX, "C", 0 };
        case PARAM_VOLT: return { "Volt", "Voltage",  VOLT_GAUGE_MIN, VOLT_GAUGE_MAX, "V", 1 };
        case PARAM_BOOST: return { "Boost", "Boost",  -100.0f,        183.0f,         "kPa", 0 };  // unit/range handled by boost code
#ifdef ENABLE_AP_PARAM
        case PARAM_AP:   return { "AP",   "AP",       AP_GAUGE_MIN,   AP_GAUGE_MAX,   "%", 0 };
#endif
        default:         return { "?",    "?",        0.0f,           1.0f,           "",  0 };
    }
}

static float paramValue(Parameter p) {
    switch (p) {
        case PARAM_ECT:  return ectValue;
        case PARAM_OIL:  return oilValue;
        case PARAM_VOLT: return voltValue;
        case PARAM_BOOST: return boostKpa;
#ifdef ENABLE_AP_PARAM
        case PARAM_AP:   return apValue;
#endif
        default:         return 0.0f;
    }
}

#ifdef ENABLE_STATS
static unsigned long paramLastUpdate(Parameter p) {
    switch (p) {
        case PARAM_ECT:  return ectLastUpdate;
        case PARAM_OIL:  return oilLastUpdate;
        case PARAM_VOLT: return voltLastUpdate;
        case PARAM_BOOST: return boostLastUpdate;
#ifdef ENABLE_AP_PARAM
        case PARAM_AP:   return apLastUpdate;
#endif
        default:         return 0;
    }
}

// Samples every parameter that has fresh data once per STATS_SAMPLE_MS (fixed
// rate so the average isn't skewed by how often each CAN stream is sent), and
// returns to the live view after STATS_VIEW_TIMEOUT_MS of no button activity.
static void updateStats() {
    unsigned long now = millis();

    if (STATS_VIEW_TIMEOUT_MS && statView != VIEW_LIVE &&
        (now - statViewSince) >= STATS_VIEW_TIMEOUT_MS) {
        statView     = VIEW_LIVE;
        displayDirty = true;
    }

    if (now - lastStatSample < STATS_SAMPLE_MS) return;
    lastStatSample = now;

    for (uint8_t i = 0; i < PARAM_COUNT; i++) {
        unsigned long t = paramLastUpdate((Parameter)i);
        if (t == 0 || (now - t) > CAN_DATA_TIMEOUT) continue;   // never seen / stale

        float v = paramValue((Parameter)i);
        Stat &st = stats[i];
        if (st.n == 0) {
            st.mn = st.mx = st.avg = v;
        } else {
            if (v < st.mn) st.mn = v;
            if (v > st.mx) st.mx = v;
            st.avg += (v - st.avg) / (float)(st.n + 1);
        }
        st.n++;
    }
}
#endif

// ============================================================================
// RENDER
// Dispatches to the correct display function based on current parameter and
// display type. The underlying library (I2C or SPI) is invisible to this code.
// ============================================================================
static void renderDisplay() {
#ifdef ENABLE_SHIFT_LIGHT
    // Shift light overrides every screen.
    if (shiftFlashing) {
        displayShiftNow(shiftPhase);
        displayDirty = false;
        return;
    }
    displaySetShiftBar(shiftBarPx);
    if (menuActive) {
        if (menuItem == 0) displayShiftMenu(menuRpm);
        else               displayUnitsMenu(menuUnit);
        displayDirty = false;
        return;
    }
#endif

    ParamCfg c = paramCfg(currentParam);
    float    value = paramValue(currentParam);
    bool     stale = isStale();
    char     label[20];

#ifdef ENABLE_STATS
    static const char* const suffix[VIEW_COUNT] = { "", " max", " min", " avg" };
    const char* base = (currentDisp == DISP_BAR) ? c.barLabel : c.textLabel;
    snprintf(label, sizeof(label), "%s%s", base, suffix[statView]);
    if (statView != VIEW_LIVE) {
        const Stat &st = stats[currentParam];
        value = (statView == VIEW_MAX) ? st.mx : (statView == VIEW_MIN) ? st.mn : st.avg;
        stale = (st.n == 0);   // Nothing recorded yet -> NO DATA
    }
#else
    snprintf(label, sizeof(label), "%s", (currentDisp == DISP_BAR) ? c.barLabel : c.textLabel);
#endif

    if (currentParam == PARAM_BOOST) {
        float       shown;
        const char* unit;
        uint8_t     dec;
        boostConvert(value, shown, unit, dec);   // value is kPa (live or stat)
        if (currentDisp == DISP_BAR) {
            displayBoostBar(label, value, shown, unit, dec, stale);
        } else {
            displayTextScreen(label, shown, unit, dec, stale);
        }
    } else if (currentDisp == DISP_BAR) {
        displayBarGraph(label, value, c.gmin, c.gmax, c.unit, c.decimals, stale);
    } else {
        displayTextScreen(label, value, c.unit, c.decimals, stale);
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
    pinMode(BUTTON2_PIN, INPUT_PULLUP);

    displayInit();
    loadView();
#ifdef ENABLE_SHIFT_LIGHT
    loadSettings();
#endif

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

#ifdef ENABLE_SHIFT_LIGHT
    updateShiftLight();
#endif

#ifdef ENABLE_STATS
    updateStats();
#endif

    // 2. Handle button input via state machine
    handleButtons();

    // 3. Redraw the display if data changed, mode changed, or the refresh
    //    interval has elapsed (ensures stale detection updates on screen too)
    unsigned long now = millis();
    if (displayDirty || (now - lastDisplayUpdate) >= DISPLAY_UPDATE_INTERVAL) {
        renderDisplay();
        lastDisplayUpdate = now;
    }
}
