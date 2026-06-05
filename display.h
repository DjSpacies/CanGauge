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

#endif // DISPLAY_H
