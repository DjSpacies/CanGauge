# CanGauge

A CAN bus gauge built to drop into the existing digital gauge cluster of my X83 Toyota. It reads live telemetry from the car's CAN bus and displays it on a small OLED, styled to match the original digital dash.

## Overview

The car runs a Link G4+ aftermarket ECU, which broadcasts engine data over CAN. CanGauge taps into that bus, decodes the frames I care about, and renders them on a 128x64 OLED.
The whole thing is small enough to hide inside the factory cluster.

Telemetry comes in over SPI via a mini MCP2515 CAN module brought from aliexpress. The firmware identifies the relevant CAN frames, extracts the signals, and pushes them to the display.

## Hardware

| Component | Detail |
|-----------|--------|
| MCU | Arduino Nano (ATmega328) |
| CAN controller | MCP2515 module over SPI (note: 8 MHz crystal variant) |
| Display | 128x64 SSD1306 OLED over hardware SPI |
| Input | Two push buttons (A on D4, B on D3), short / long press |
| Power | 12V → 5V via LM2596 buck converter |
| ECU | Link G4+ broadcasting on CAN at 250 kbps |

## Features

Parameters:

- Engine Coolant Temperature (ECT)
- Oil Temperature
- Battery Voltage
- Boost / vacuum (MGP) - zero-centred bar with zero about a third from the left to show more boost than vacuum. Selectable units: kPa, bar, or psi (boost) / inHg (vacuum)
- Accelerator position (AP) - temporary latency-test parameter

Each can be shown in one of two display modes:

- **Bar graph** - emulates the original digital dash style, with added numerical readout.
- **Numerical** - a large, clean readout (e.g. `13.0V`).

The firmware flags stale data - if a signal stops updating on the bus, the gauge shows a "NO DATA" indicator rather than displaying a frozen value.

### Shift light

Driven from engine RPM (CAN stream 1) with its own logic, since the gauge consumes telemetry over CAN rather than using an ECU output. It overrides whatever screen is showing:

- A solid bar sweeps left to right across the last 500 rpm below the shift point.
- At the shift point the screen flashes **SHIFT** (text and background inverting).
- It is disabled below 2000 rpm and when RPM data goes stale, and uses hysteresis so it does not flicker at the threshold.
- The shift point is adjustable from 6000 to 8000 rpm in the menu and stored in EEPROM.

For bench testing without a running engine, `SHIFT_TEST_USE_AP` in `config.h` drives the shift light from the accelerator position instead.

### Warnings

ECT, oil temperature and boost each have an over-limit warning: a full-screen flashing alert (e.g. `ECT HIGH 102C`, `OVER BOOST 18.5psi`) that interrupts whatever is showing. The shift light flash takes priority over warnings, and the menu is never interrupted.

- Defaults: ECT 100 C, oil 130 C, boost 124 kPa (about 18 psi). All are adjustable in the menu and stored in EEPROM.
- A warning clears with hysteresis (3 C / 10 kPa below the limit) and only counts fresh CAN data.
- Any button press dismisses the warning on screen (and does nothing else). It returns if the condition is still present after 30 seconds, or on the next excursion.

### Min / max / average

A short press on button B cycles the current screen between live, max, min and average (sampled once a second, reset at power-up). It returns to live after 10 seconds without a button press so a held value is never mistaken for a live one.

### Controls

| Input | Normal | In menu |
|-------|--------|---------|
| A short | Next parameter | Decrease / previous |
| A long | Toggle bar / numerical | Cancel and exit |
| B short | Cycle live / max / min / avg | Increase / next |
| B long | Open menu | Next item, then save and exit |

Menu items, in order: shift point, ECT warning, oil warning, boost units, boost warning. B long steps to the next item and saves on the last one; A long cancels.

The last parameter and bar / numerical mode are remembered across power cycles (stored in EEPROM). The menu and min / max / average views are not, so the gauge always boots into a live view.

## Configuration

Hardware pins, CAN IDs, gauge ranges and display options all live in `config.h`, so the firmware can be adapted to a different car or wiring without touching the main code. The CAN stream IDs and signal positions are set up to match my Link G4+ transmit configuration and will need adjusting for other setups.

## Building & Flashing

Built with [`arduino-cli`](https://arduino.github.io/arduino-cli/). You'll need the AVR core and two libraries:
```bash
arduino-cli core install arduino:avr
arduino-cli lib install "U8g2"
arduino-cli lib install "CAN_BUS_Shield"
```
Compile and upload (adjust the port to suit):
```bash
arduino-cli compile -u -p COM3 --fqbn arduino:avr:nano:cpu=atmega328 .
```
If you're using an MCP2515 module with an 8 MHz crystal (some of them use 16MHz), make sure `CAN_CLOCK` is set to `MCP_8MHz` in `config.h` — a mismatch here lets the controller initialise cleanly but it'll never actually receive a frame, which is a fun afternoon to debug.

## Roadmap

- Optimise flash usage to free up headroom
- Reset min/max/avg from the menu
- Transmit inputs back to the Link ECU via a menu e.g select boost level, toggle Intercooler Spray etc.
