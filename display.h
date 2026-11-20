#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include "config.h"

// ============================================================================
// DISPLAY ABSTRACTION INTERFACE
//
// All functions are implemented in either display_i2c.cpp or display_spi.cpp
// depending on the DISPLAY_DRIVER_* define in config.h.
//
// CanGauge.ino calls only these functions — it never touches a display
// library directly. Swapping hardware requires only a config.h change.
// ============================================================================

// Initialise display hardware. Call once from setup() before anything else.
void displayInit();

// Play the splash screen animation. Call once from setup() after displayInit().
void displaySplash();

// Draw a bar graph screen for the given parameter.
//   label    — short label shown at top centre  e.g. "ECT", "Oil", "Volt"
//   value    — current reading
//   minVal   — minimum of the gauge scale
//   maxVal   — maximum of the gauge scale
//   unit     — unit suffix appended to the value  e.g. "C", "V"
//   decimals — decimal places to render (0 for temperature, 1 for voltage)
//   stale    — when true a "NO DATA" warning is overlaid on the screen
void displayBarGraph(const char* label,
                     float       value,
                     float       minVal,
                     float       maxVal,
                     const char* unit,
                     uint8_t     decimals,
                     bool        stale);

// Draw a large-text screen for the given parameter.
//   label    — descriptor shown small at the top  e.g. "ECT", "Oil Temp"
//   value    — current reading
//   unit     — unit suffix
//   decimals — decimal places
//   stale    — when true a "NO DATA" warning is overlaid
void displayTextScreen(const char* label,
                       float       value,
                       const char* unit,
                       uint8_t     decimals,
                       bool        stale);

// Shift light ramp overlay. A solid full-height bar of this width
// (0..SCREEN_WIDTH px, 0 = off) is drawn left to right on top of whichever
// screen is showing.
void displaySetShiftBar(uint8_t widthPx);

// Full-screen "SHIFT NOW" screen. inverted flips text/background so calling
// it alternately with true/false produces the flash.
void displayShiftNow(bool inverted);

// Boost / vacuum bar graph. The bar is zero-centred (vacuum grows left from
// zero, boost grows right) and is always scaled in kPa, whatever unit is shown.
//   kpa     — gauge pressure in kPa, drives the bar
//   shown   — the same pressure converted to the display unit, for the readout
//   unit    — unit suffix for the readout ("kPa", "bar", "psi", "inHg")
void displayBoostBar(const char* label,
                     float       kpa,
                     float       shown,
                     const char* unit,
                     uint8_t     decimals,
                     bool        stale);

// Generic numeric menu screen: small title, big value, unit, button hints.
// last = this is the final menu item (hint says OK/save rather than NEXT).
void displayMenuValue(const char* title, const char* value, const char* unit, bool last);

// Boost units menu screen. sel = 0 kPa, 1 bar, 2 psi/inHg.
void displayUnitsMenu(uint8_t sel, bool last);

// Full-screen warning, e.g. label "ECT HIGH", value 102, unit "C". inverted
// flips text/background so alternating it produces the flash.
void displayWarning(const char* label, float value, const char* unit,
                    uint8_t decimals, bool inverted);

#endif // DISPLAY_H
