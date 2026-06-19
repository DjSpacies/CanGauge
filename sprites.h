#ifndef SPRITES_H
#define SPRITES_H

#include <Arduino.h>

// XBM format (LSB-first). Render with U8g2 drawXBMP(x, y, w, h, data).
// These are the original Adafruit MSB-first sprites with each byte
// bit-reversed so they display the correct way round under U8g2.

// Textured gauge bar segment: 6px wide, 21px tall.
static const uint8_t barSprite[] PROGMEM = {
    0x1f, 0x00, 0x00, 0x1f, 0x00, 0x00, 0x1f,
    0x1f, 0x00, 0x00, 0x1f, 0x1f, 0x1f, 0x00,
    0x1f, 0x1f, 0x1f, 0x00, 0x1f, 0x1f, 0x1f
};

// Centre marker sprite: 10px wide, 9px tall, 2 bytes per row.
static const uint8_t bitmapBottom[] PROGMEM = {
    0x10, 0x00,
    0x70, 0x00,
    0x10, 0x00,
    0x70, 0x00,
    0x10, 0x00,
    0x13, 0x03,
    0xdc, 0x00,
    0x33, 0x03,
    0xcc, 0x00
};

#endif // SPRITES_H