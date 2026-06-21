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
| Input | Push button (short press / long press) |
| Power | 12V → 5V via LM2596 buck converter |
| ECU | Link G4+ broadcasting on CAN at 250 kbps |

## Features

Three parameters are implemented so far:

- Engine Coolant Temperature (ECT)
- Oil Temperature
- Battery Voltage

Each can be shown in one of two display modes:

- **Bar graph** — emulates the original digital dash style, with added numerical readout.
- **Numerical** — a large, clean readout (e.g. `13.0V`).

A single push button drives the UI: a **short press** cycles through the parameters, and a **long press** toggles between bar and numerical display. All button debouncing is handled in software via a small state machine.

The firmware also flags stale data — if a signal stops updating on the bus, the gauge shows a "NO DATA" indicator rather than displaying a frozen value.

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
- Second push button for min/max/avg hold (so I can check how hot things got after a session)
- Transmit inputs back to the Link ECU via a menu e.g select boost level, toggle Intercooler Spray etc.
