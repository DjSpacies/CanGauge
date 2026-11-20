#include "config.h"
#ifdef DISPLAY_DRIVER_SPI

#include "display.h"
#include "sprites.h"
#include <SPI.h>
#include <U8g2lib.h>

// ============================================================================
// DRIVER INSTANCE
// "_1_" suffix = page-buffer mode (~128 bytes RAM vs 1024 for full buffer).
// Hardware SPI uses Nano pins: MOSI = 11, SCK = 13 (automatic, no define).
// ============================================================================
static U8G2_SSD1306_128X64_NONAME_1_4W_HW_SPI oled(
    U8G2_R0,
    DISP_CS_PIN,
    DISP_DC_PIN,
    DISP_RST_PIN
);

// ============================================================================
// INTERNAL HELPERS
// ============================================================================

static uint8_t valueToBarPixels(float value, float minVal, float maxVal) {
    if (value <= minVal) return 0;
    if (value >= maxVal) return GAUGE_LENGTH;
    return (uint8_t)(((value - minVal) / (maxVal - minVal)) * GAUGE_LENGTH);
}

static void formatValue(char* buf, uint8_t bufLen, float value, uint8_t decimals) {
    if (decimals == 0) {
        itoa((int)value, buf, 10);   // (snprintf avoided: pulls in all of vfprintf)
    } else {
        dtostrf(value, 4, decimals, buf);
        char* p = buf;
        while (*p == ' ') p++;
        if (p != buf) memmove(buf, p, strlen(p) + 1);
    }
}

// "label: value" + unit, e.g. "Oil max: 98C". Plain string ops, bounds-checked.
static void buildLine(char* dst, uint8_t cap, const char* label,
                      const char* value, const char* unit) {
    dst[0] = 0;
    strncat(dst, label, cap - 1);
    strncat(dst, ": ",  cap - 1 - strlen(dst));
    strncat(dst, value, cap - 1 - strlen(dst));
    strncat(dst, unit,  cap - 1 - strlen(dst));
}

// Draw bar chrome. showEnds = draw the C/H cold/hot markers (temperature only).
static void drawBarChrome(bool isTemp) {
    oled.drawHLine(X_PADDING + 3, SCREEN_HEIGHT - 16,
                   SCREEN_WIDTH - ((X_PADDING + 4) * 2) - 1);
    oled.drawHLine(X_PADDING + 3, SCREEN_HEIGHT - 14,
                   SCREEN_WIDTH - ((X_PADDING + 4) * 2) - 1);

    for (int x = X_PADDING; x < SCREEN_WIDTH - X_PADDING; x += 4) {
        bool    isEnd   = (x == X_PADDING || x == SCREEN_WIDTH - X_PADDING - 4);
        uint8_t tickTop = isEnd ? 14 : 16;
        uint8_t tickH   = isEnd ? 8  : 6;
        uint8_t tickW   = isEnd ? 2  : 1;
        oled.drawBox(x, tickTop, tickW, tickH);
    }

    if (isTemp) {
        oled.setFont(u8g2_font_6x10_tf);
        oled.drawStr(X_PADDING - 6, 12, "C");
        oled.drawStr(SCREEN_WIDTH - X_PADDING - 2, 12, "H");
        oled.drawXBMP((SCREEN_WIDTH / 2) - 5, SCREEN_HEIGHT - 10, 10, 9, bitmapBottom);
    }
}

static void drawBarFill(uint8_t barPixels) {
    for (uint8_t i = 2; i < barPixels; i++) {
        oled.drawXBMP(i * BAR_WIDTH, 25, BAR_WIDTH, BAR_HEIGHT, barSprite);
    }
}

// Draws a string in 6x10 font, but renders ':' and '.' as minimal dots
// instead of the font's chunky cross-shaped glyphs. Fixed 6px advance.
static void drawCleanStr(int x, int y, const char* s) {
    oled.setFont(u8g2_font_6x10_tf);
    char tmp[2] = {0, 0};
    while (*s) {
        if (*s == ':') {
            oled.drawPixel(x + 2, y - 5);
            oled.drawPixel(x + 2, y - 3);
        } else if (*s == '.') {
            oled.drawPixel(x + 2, y - 1);
        } else {
            tmp[0] = *s;
            oled.drawStr(x, y, tmp);
        }
        x += 6;
        s++;
    }
}

// Shift light ramp: solid full-height bar growing left to right, drawn at the
// end of every page pass of every screen. XOR mode (draw color 2) so whatever
// is underneath stays readable (inverted) instead of being blotted out.
static uint8_t shiftBarPx = 0;

static void drawShiftOverlay() {
    if (!shiftBarPx) return;
    oled.setDrawColor(2);
    oled.drawBox(0, 0, shiftBarPx, SCREEN_HEIGHT);
    oled.setDrawColor(1);
}

// Blocky letter built from rectangles (no font data). Supports S H I F T.
// x,y = top-left, w = width, h = height, t = stroke thickness.
static void drawBlockLetter(char c, int x, int y, int w, int h, int t) {
    int midY = y + (h - t) / 2;
    switch (c) {
        case 'S':
            oled.drawBox(x, y, w, t);                         // top
            oled.drawBox(x, midY, w, t);                      // middle
            oled.drawBox(x, y + h - t, w, t);                 // bottom
            oled.drawBox(x, y, t, midY + t - y);              // upper left
            oled.drawBox(x + w - t, midY, t, y + h - midY);   // lower right
            break;
        case 'H':
            oled.drawBox(x, y, t, h);
            oled.drawBox(x + w - t, y, t, h);
            oled.drawBox(x, midY, w, t);
            break;
        case 'I':
            oled.drawBox(x, y, t, h);
            break;
        case 'F':
            oled.drawBox(x, y, t, h);
            oled.drawBox(x, y, w, t);
            oled.drawBox(x, midY, w - t, t);
            break;
        case 'T':
            oled.drawBox(x, y, w, t);
            oled.drawBox(x + (w - t) / 2, y, t, h);
            break;
    }
}

// ============================================================================
// PUBLIC INTERFACE
// ============================================================================

void displaySetShiftBar(uint8_t widthPx) {
    shiftBarPx = (widthPx > SCREEN_WIDTH) ? SCREEN_WIDTH : widthPx;
}

static void drawMenuHint(bool last) {
    oled.setFont(u8g2_font_6x10_tf);
    oled.drawStr(0, SCREEN_HEIGHT - 1, last ? "A- B+ hold A=X B=OK" : "A- B+ hold A=X B=NEXT");
}

void displayMenuValue(const char* title, const char* value, const char* unit, bool last) {
    oled.firstPage();
    do {
        oled.setFont(u8g2_font_6x10_tf);
        oled.drawStr(0, 10, title);

        oled.setFont(u8g2_font_logisoso28_tn);
        uint8_t numW = oled.getStrWidth(value);
        oled.setFont(u8g2_font_6x10_tf);
        uint8_t totalW = numW + 2 + oled.getStrWidth(unit);
        uint8_t startX = (SCREEN_WIDTH - totalW) / 2;

        oled.setFont(u8g2_font_logisoso28_tn);
        oled.drawStr(startX, 46, value);
        oled.setFont(u8g2_font_6x10_tf);
        oled.drawStr(startX + numW + 2, 46, unit);

        drawMenuHint(last);
        drawShiftOverlay();
    } while (oled.nextPage());
}

void displayShiftNow(bool inverted) {
    oled.firstPage();
    do {
        // 5 letters: S H F T are 20px wide, I is 6px; 6px gaps; stroke 6px.
        const int LW = 20, LT = 6, LH = 36, GAP = 6, LY = (SCREEN_HEIGHT - LH) / 2;
        const int totalW = 4 * LW + LT + 4 * GAP;
        if (inverted) {
            oled.setDrawColor(1);
            oled.drawBox(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
            oled.setDrawColor(0);
        } else {
            oled.setDrawColor(1);
        }
        int x = (SCREEN_WIDTH - totalW) / 2;
        for (const char* p = "SHIFT"; *p; p++) {
            int w = (*p == 'I') ? LT : LW;
            drawBlockLetter(*p, x, LY, w, LH, LT);
            x += w + GAP;
        }
        oled.setDrawColor(1);
    } while (oled.nextPage());
}

void displayInit() {
    oled.begin();
    oled.setContrast(200);
    oled.clearBuffer();
    oled.sendBuffer();
    Serial.println(F("SSD1306 SPI OK"));
}

void displayBarGraph(const char* label,
                     float       value,
                     float       minVal,
                     float       maxVal,
                     const char* unit,
                     uint8_t     decimals,
                     bool        stale) {

    uint8_t barPixels = valueToBarPixels(value, minVal, maxVal);

    // Show C/H end markers only for temperature gauges (unit C or F).
    bool isTemp = (unit && (unit[0] == 'C' || unit[0] == 'F'));

    char valBuf[12];
    formatValue(valBuf, sizeof(valBuf), value, decimals);
    char line[28];
    buildLine(line, sizeof(line), label, valBuf, unit);

    oled.firstPage();
    do {
        drawBarChrome(isTemp);
        drawBarFill(barPixels);

        // 5x7 font for the title — already loaded (no extra flash) and has
        // cleaner punctuation than 6x10. Revert to 6x10 if you prefer.
        uint8_t w = strlen(line) * 6;          // 6x10 is fixed-width
        drawCleanStr((SCREEN_WIDTH - w) / 2, 10, line);

        if (stale) {
            oled.setFont(u8g2_font_6x10_tf);
            oled.drawStr(0, SCREEN_HEIGHT - 1, "NO DATA");
        }
        drawShiftOverlay();
    } while (oled.nextPage());
}

void displayBoostBar(const char* label,
                     float       kpa,
                     float       shown,
                     const char* unit,
                     uint8_t     decimals,
                     bool        stale) {

    // Segment grid matches drawBarFill (segment i spans x = i*BAR_WIDTH). The
    // zero segment boundary is where vacuum (left) meets boost (right).
    const int8_t firstSeg = 2;
    const int8_t zeroSeg  = firstSeg + BOOST_VAC_SEGMENTS;

    float f = kpa / BOOST_KPA_PER_SEGMENT;
    if (f >  BOOST_BOOST_SEGMENTS) f =  BOOST_BOOST_SEGMENTS;
    if (f < -BOOST_VAC_SEGMENTS)   f = -BOOST_VAC_SEGMENTS;
    int8_t n = (int8_t)f;                 // whole segments, toward zero

    char valBuf[12];
    formatValue(valBuf, sizeof(valBuf), shown, decimals);
    char line[28];
    buildLine(line, sizeof(line), label, valBuf, unit);

    oled.firstPage();
    do {
        drawBarChrome(false);

        if (n > 0) {
            for (int8_t i = zeroSeg; i < zeroSeg + n; i++)
                oled.drawXBMP(i * BAR_WIDTH, 25, BAR_WIDTH, BAR_HEIGHT, barSprite);
        } else if (n < 0) {
            for (int8_t i = zeroSeg + n; i < zeroSeg; i++)
                oled.drawXBMP(i * BAR_WIDTH, 25, BAR_WIDTH, BAR_HEIGHT, barSprite);
        }

        // Zero marker: heavy tick above the bar and a "0" below it.
        oled.drawBox(zeroSeg * BAR_WIDTH - 1, 14, 2, 10);
        oled.setFont(u8g2_font_6x10_tf);
        oled.drawStr(zeroSeg * BAR_WIDTH - 3, SCREEN_HEIGHT - 1, "0");

        uint8_t w = strlen(line) * 6;
        drawCleanStr((SCREEN_WIDTH - w) / 2, 10, line);

        if (stale) {
            oled.setFont(u8g2_font_6x10_tf);
            oled.drawStr(0, SCREEN_HEIGHT - 1, "NO DATA");
        }
        drawShiftOverlay();
    } while (oled.nextPage());
}

void displayUnitsMenu(uint8_t sel, bool last) {
    static const char* const names[3] = { "kPa", "bar", "psi / inHg" };

    oled.firstPage();
    do {
        oled.setFont(u8g2_font_6x10_tf);
        oled.drawStr(0, 10, "BOOST UNITS");
        for (uint8_t i = 0; i < 3; i++) {
            uint8_t y = 26 + i * 12;
            if (i == sel) oled.drawStr(0, y, ">");
            oled.drawStr(12, y, names[i]);
        }
        drawMenuHint(last);
        drawShiftOverlay();
    } while (oled.nextPage());
}

// Shared by the normal text screen and the warning screen. inverted = white
// background with black text (the warning flash).
static void drawText(const char* label,
                     float       value,
                     const char* unit,
                     uint8_t     decimals,
                     bool        stale,
                     bool        inverted) {

    char valBuf[12];
    formatValue(valBuf, sizeof(valBuf), value, decimals);

    oled.firstPage();
    do {
        if (inverted) {
            oled.setDrawColor(1);
            oled.drawBox(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
            oled.setDrawColor(0);
        }

        // Small label at top
        oled.setFont(u8g2_font_6x10_tf);
        oled.drawStr(0, 10, label);

        // Big number in the number-only font (no letters = much smaller).
        oled.setFont(u8g2_font_logisoso28_tn);
        uint8_t numW = oled.getStrWidth(valBuf);

        // Unit in the small font, measured for combined centring.
        oled.setFont(u8g2_font_6x10_tf);
        uint8_t unitW  = oled.getStrWidth(unit);
        uint8_t totalW = numW + 2 + unitW;
        uint8_t startX = (SCREEN_WIDTH - totalW) / 2;

        oled.setFont(u8g2_font_logisoso28_tn);
        oled.drawStr(startX, 58, valBuf);

        oled.setFont(u8g2_font_6x10_tf);
        oled.drawStr(startX + numW + 2, 58, unit);

        if (stale) {
            oled.drawStr(0, SCREEN_HEIGHT - 1, "NO DATA");
        }
        oled.setDrawColor(1);
        if (!inverted) drawShiftOverlay();
    } while (oled.nextPage());
}

void displayTextScreen(const char* label,
                       float       value,
                       const char* unit,
                       uint8_t     decimals,
                       bool        stale) {
    drawText(label, value, unit, decimals, stale, false);
}

void displayWarning(const char* label, float value, const char* unit,
                    uint8_t decimals, bool inverted) {
    drawText(label, value, unit, decimals, false, inverted);
}

#endif // DISPLAY_DRIVER_SPI
