#include "config.h"
#ifdef DISPLAY_DRIVER_SPI

#include "display.h"
#include "splash.h"
#include "sprites.h"          // <-- recovered dash sprites
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
        snprintf(buf, bufLen, "%d", (int)value);
    } else {
        dtostrf(value, 4, decimals, buf);
        char* p = buf;
        while (*p == ' ') p++;
        if (p != buf) memmove(buf, p, strlen(p) + 1);
    }
}

// Draw bar chrome. showEnds = draw the C/H cold/hot markers (temperature only).
static void drawBarChrome(bool showEnds) {
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

    if (showEnds) {
        oled.setFont(u8g2_font_6x10_tf);
        oled.drawStr(X_PADDING - 6, 12, "C");
        oled.drawStr(SCREEN_WIDTH - X_PADDING - 2, 12, "H");
    }

    // Centre marker sprite (XBM, drawn LSB-first).
    oled.drawXBMP((SCREEN_WIDTH / 2) - 5, SCREEN_HEIGHT - 10, 10, 9, bitmapBottom);
}

static void drawBarFill(uint8_t barPixels) {
    for (uint8_t i = 2; i < barPixels; i++) {
        oled.drawXBMP(i * BAR_WIDTH, 25, BAR_WIDTH, BAR_HEIGHT, barSprite);
    }
}

// ============================================================================
// PUBLIC INTERFACE
// ============================================================================

void displayInit() {
    oled.begin();
    oled.setContrast(200);
    oled.clearBuffer();
    oled.sendBuffer();
    Serial.println(F("SSD1306 SPI OK"));
}

void displaySplash() {
#if ENABLE_SPLASH_SCREEN
    for (int i = 0; i < epd_bitmap_allArray_LEN; i++) {
        oled.firstPage();
        do {
            oled.drawXBMP(0, 0, 128, 32, epd_bitmap_allArray[i]);
        } while (oled.nextPage());
        delay(2);
    }
    oled.clearBuffer();
    oled.sendBuffer();
#endif
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
    snprintf(line, sizeof(line), "%s: %s%s", label, valBuf, unit);

    oled.firstPage();
    do {
        drawBarChrome(isTemp);
        drawBarFill(barPixels);

        // 5x7 font for the title — already loaded (no extra flash) and has
        // cleaner punctuation than 6x10. Revert to 6x10 if you prefer.
        oled.setFont(u8g2_font_5x7_tf);
        uint8_t w = oled.getStrWidth(line);
        oled.drawStr((SCREEN_WIDTH - w) / 2, 9, line);

        if (stale) {
            oled.setFont(u8g2_font_5x7_tf);
            oled.drawStr(0, SCREEN_HEIGHT - 1, "NO DATA");
        }
    } while (oled.nextPage());
}

void displayTextScreen(const char* label,
                       float       value,
                       const char* unit,
                       uint8_t     decimals,
                       bool        stale) {

    char valBuf[12];
    formatValue(valBuf, sizeof(valBuf), value, decimals);
    char line[20];
    snprintf(line, sizeof(line), "%s%s", valBuf, unit);

    oled.firstPage();
    do {
        oled.setFont(u8g2_font_6x10_tf);
        oled.drawStr(0, 10, label);

        oled.setFont(u8g2_font_logisoso28_tf);
        uint8_t w = oled.getStrWidth(line);
        oled.drawStr((SCREEN_WIDTH - w) / 2, 58, line);

        if (stale) {
            oled.setFont(u8g2_font_5x7_tf);
            oled.drawStr(0, SCREEN_HEIGHT - 1, "NO DATA");
        }
    } while (oled.nextPage());
}

#endif // DISPLAY_DRIVER_SPI