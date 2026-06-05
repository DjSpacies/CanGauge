#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include "config.h"

// ============================================================================
// DISPLAY ABSTRACTION INTERFACE
// All functions are implemented in either display_i2c.cpp or display_spi.cpp
// depending on the DISPLAY_DRIVER_* define in config.h.
// The main sketch calls only these — never library functions directly.
// ============================================================================

// Initialise the display hardware. Call once from setup().
void displayInit();

// Show the splash screen animation. Call once from setup() after displayInit().
void displaySplash();

// Draw a bar graph screen.
// label   — short string shown at top (e.g. "ECT", "Oil", "Volt")
// value   — current float value
// minVal  — minimum of gauge range
// maxVal  — maximum of gauge range
// unit    — unit string appended to value (e.g. "C", "V")
// decimals — decimal places to show (0 for temp, 1 for volts)
// stale   — if true, a "NO DATA" warning is overlaid
void displayBarGraph(const char* label,
                     float value,
                     float minVal,
                     float maxVal,
                     const char* unit,
                     uint8_t decimals,
                     bool stale);

// Draw a large-text screen.
// label    — descriptor shown small at top
// value    — current float value
// unit     — unit string
// decimals — decimal places (0 for temp, 1 for volts)
// stale    — if true, a "NO DATA" warning is overlaid
void displayTextScreen(const char* label,
                       float value,
                       const char* unit,
                       uint8_t decimals,
                       bool stale);

#endif // DISPLAY_H
