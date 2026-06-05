#include "config.h"
#ifdef DISPLAY_DRIVER_I2C

#include "display.h"
#include "splash.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ============================================================================
// DRIVER INSTANCE
// ============================================================================
static Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ============================================================================
// INTERNAL HELPERS
// ============================================================================

// The only place oled.display() is called — never mid-draw.
static inline void commit() {
    oled.display();
}

// Map a value onto the bar pixel length.
static uint8_t valueToBarPixels(float value, float minVal, float maxVal) {
    if (value <= minVal) return 0;
    if (value >= maxVal) return GAUGE_LENGTH;
    return (uint8_t)(((value - minVal) / (maxVal - minVal)) * GAUGE_LENGTH);
}

// Format a float into a char buffer, trimming any leading spaces dtostrf adds.
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

// Draw the bar graph scale chrome: accent lines, tick marks, C / H labels.
static void drawBarChrome() {
    // Two horizontal accent lines forming the bottom of the scale
    oled.drawFastHLine(X_PADDING + 3, SCREEN_HEIGHT - 16,
                       SCREEN_WIDTH - ((X_PADDING + 4) * 2) - 1, WHITE);
    oled.drawFastHLine(X_PADDING + 3, SCREEN_HEIGHT - 14,
                       SCREEN_WIDTH - ((X_PADDING + 4) * 2) - 1, WHITE);

    // Tick marks — taller and wider at the two ends
    for (int x = X_PADDING; x < SCREEN_WIDTH - X_PADDING; x += 4) {
        bool    isEnd   = (x == X_PADDING || x == SCREEN_WIDTH - X_PADDING - 4);
        uint8_t tickTop = isEnd ? 14 : 16;
        uint8_t tickH   = isEnd ? 8  : 6;
        uint8_t tickW   = isEnd ? 2  : 1;
        oled.fillRect(x, tickTop, tickW, tickH, WHITE);
    }

    // Cold / hot end labels
    oled.setTextSize(1);
    oled.setTextColor(WHITE);
    oled.setCursor(X_PADDING - 6, 2);
    oled.print('C');
    oled.setCursor(SCREEN_WIDTH - X_PADDING - 2, 2);
    oled.print('H');
}

// Draw filled bar segments for the given pixel length.
static void drawBarFill(uint8_t barPixels) {
    for (uint8_t i = 2; i < barPixels; i++) {
        oled.fillRect(i * BAR_WIDTH, 25, BAR_WIDTH - 1, BAR_HEIGHT, WHITE);
    }
}

// ============================================================================
// PUBLIC INTERFACE
// ============================================================================

void displayInit() {
    Wire.begin();
    Wire.setClock(I2C_CLOCK_SPEED);   // 400kHz Fast Mode

    if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDRESS)) {
        Serial.println(F("SSD1306 I2C init failed — halting"));
        for (;;);
    }
    oled.clearDisplay();
    oled.display();
    Serial.println(F("SSD1306 I2C OK"));
}

void displaySplash() {
#if ENABLE_SPLASH_SCREEN
    for (int i = 0; i < epd_bitmap_allArray_LEN; i++) {
        oled.clearDisplay();
        oled.drawBitmap(0, 0, epd_bitmap_allArray[i], 128, 32, WHITE);
        oled.display();
        delay(2);
    }
    oled.clearDisplay();
    oled.display();
#endif
}

void displayBarGraph(const char* label,
                     float       value,
                     float       minVal,
                     float       maxVal,
                     const char* unit,
                     uint8_t     decimals,
                     bool        stale) {

    // Build the entire frame in the Adafruit buffer first,
    // then push it all in one commit() call to avoid tearing.
    oled.clearDisplay();

    drawBarChrome();
    drawBarFill(valueToBarPixels(value, minVal, maxVal));

    // Centred label + value at the top
    char valBuf[12];
    formatValue(valBuf, sizeof(valBuf), value, decimals);
    char line[28];
    snprintf(line, sizeof(line), "%s: %s%s", label, valBuf, unit);

    oled.setTextSize(1);
    oled.setTextColor(WHITE);
    int16_t  x1, y1;
    uint16_t w, h;
    oled.getTextBounds(line, 0, 0, &x1, &y1, &w, &h);
    oled.setCursor((SCREEN_WIDTH - (int16_t)w) / 2, 4);
    oled.print(line);

    if (stale) {
        oled.setTextSize(1);
        oled.setCursor(0, SCREEN_HEIGHT - 8);
        oled.print(F("NO DATA"));
    }

    commit();
}

void displayTextScreen(const char* label,
                       float       value,
                       const char* unit,
                       uint8_t     decimals,
                       bool        stale) {

    oled.clearDisplay();

    // Small label at top left
    oled.setTextSize(1);
    oled.setTextColor(WHITE);
    oled.setCursor(0, 0);
    oled.print(label);

    // Large value + unit centred in the lower portion of the screen
    char valBuf[12];
    formatValue(valBuf, sizeof(valBuf), value, decimals);
    char line[20];
    snprintf(line, sizeof(line), "%s%s", valBuf, unit);

    oled.setTextSize(3);
    int16_t  x1, y1;
    uint16_t w, h;
    oled.getTextBounds(line, 0, 0, &x1, &y1, &w, &h);
    oled.setCursor((SCREEN_WIDTH - (int16_t)w) / 2, 28);
    oled.print(line);

    if (stale) {
        oled.setTextSize(1);
        oled.setCursor(0, SCREEN_HEIGHT - 8);
        oled.print(F("NO DATA"));
    }

    commit();
}

#endif // DISPLAY_DRIVER_I2C
