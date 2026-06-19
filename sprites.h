#ifndef SPRITES_H
#define SPRITES_H

#include <Arduino.h>

// MSB-first bitmaps (bit 7 = leftmost pixel), recovered from the original
// dash-mimic design. Render with U8g2 drawBitmap(x, y, bytesPerRow, h, data)
// -- NOT drawXBMP, which is LSB-first and would mirror these horizontally.

// Tapered/textured gauge bar segment: 6px wide, 21px tall, 1 byte per row.
static const uint8_t barSprite[] PROGMEM = {
    0xf8, 0x00, 0x00, 0xf8, 0x00, 0x00, 0xf8,
    0xf8, 0x00, 0x00, 0xf8, 0xf8, 0xf8, 0x00,
    0xf8, 0xf8, 0xf8, 0x00, 0xf8, 0xf8, 0xf8
};

// Centre marker sprite: 10px wide, 9px tall, 2 bytes per row.
static const uint8_t bitmapBottom[] PROGMEM = {
    0x08, 0x00,
    0x0e, 0x00,
    0x08, 0x00,
    0x0e, 0x00,
    0x08, 0x00,
    0xc8, 0xc0,
    0x3b, 0x00,
    0xcc, 0xc0,
    0x33, 0x00
};

#endif // SPRITES_H