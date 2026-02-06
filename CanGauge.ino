#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Encoder.h>
#include "mcp2515_can.h"
#include "splash.h"
#define CAN_2515

const int SPI_CS_PIN = 10;
const int CAN_INT_PIN = 2;
//const int GAUGE_PARAMETER_PIN = 3;
//const int DISPLAY_MODE_PIN = 4;
//Rotary encoder setup
const int encoderCLK = 2;
const int encoderDT = 3;
const int buttonPin = 4;
Encoder myEncoder(encoderCLK, encoderDT);
long encoderPosition = 0;
boolean lastCLK = LOW;
volatile boolean buttonState, lastButtonState = LOW;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;

mcp2515_can CAN(SPI_CS_PIN);

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define X_PADDING 8

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Gauge Logic and Params
enum Parameter { ECT,
                 OilTemp,
                 Voltage };
Parameter currentParam = ECT;
float tempValue = 0.0;
float voltValue = 0.0;

//Display Parameters and Sprites
// Custom bitmaps for "C" and "H"
// C char
const unsigned char bitmapC[] PROGMEM = {
  0x7e,
  0xff,
  0xc3,
  0xc3,
  0xc0,
  0xc0,
  0xc3,
  0xc3,
  0xff,
  0x7e
};
// H char
const unsigned char bitmapH[] PROGMEM = {
  0xc3,
  0xc3,
  0xc3,
  0xc3,
  0xff,
  0xff,
  0xc3,
  0xc3,
  0xc3,
  0xc3
};

// Routine to draw C and H characters
void drawBitmap(int x, int y, const unsigned char* bitmap, int width, int height) {
  display.drawBitmap(x, y, bitmap, width, height, WHITE);
}

// Custom bitmap for temperature sprite
const unsigned char bitmapBottom[] PROGMEM = {
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

#define BAR_WIDTH 6      // Width of the bar sprite
#define BAR_HEIGHT 21    // Height of the bar sprite
#define GAUGE_LENGTH 19  // Length of the gauge in pixels
// Bar Graph sprite
const unsigned char barSprite[] PROGMEM = {
  0xf8,
  0x00,
  0x00,
  0xf8,
  0x00,
  0x00,
  0xf8,
  0xf8,
  0x00,
  0x00,
  0xf8,
  0xf8,
  0xf8,
  0x00,
  0xf8,
  0xf8,
  0xf8,
  0x00,
  0xf8,
  0xf8,
  0xf8
};

// Flag to switch between bar graph and text display
bool displayAsBar = true;

//Cycle through Gauge display variables
void switchParameter() {
  currentParam = static_cast<Parameter>((currentParam + 1) % 3);
}

void drawVerticalBar(int x, int y, const unsigned char* sprite, int width, int height) {
  display.drawBitmap(x, y, sprite, width, height, WHITE);
}

//Bar Graph Display Mode
void displayBarGraph(int gaugeValue, float voltValue, Parameter currentParam) {
  display.clearDisplay();
  // Draw a solid line at the bottom of the screen (2 pixels high)
  for (int i = 0; i < 2; ++i) {
    display.drawFastHLine(X_PADDING + 3, SCREEN_HEIGHT - i - 16, SCREEN_WIDTH - ((X_PADDING + 4) * 2) - 1, WHITE);
  }

  // Draw second accent line below
  display.drawFastHLine(X_PADDING + 3, SCREEN_HEIGHT - 14, SCREEN_WIDTH - ((X_PADDING + 4) * 2) - 1, WHITE);

  // Draw varied width and height divisions
  for (int i = X_PADDING; i < SCREEN_WIDTH - X_PADDING; i += 4) {  // Adjust the increment to preferred spacing
    if (i == X_PADDING || i == SCREEN_WIDTH - X_PADDING - 4) {     // First and last divisions
      for (int j = 14; j < 22; ++j) {                              // Adjust the height of the first and last divisions (8 pixels)
        for (int k = i; k < i + 2; ++k) {                          // Set the width to 2 pixels for the first and last divisions
          display.drawPixel(k, j, WHITE);
          display.drawPixel(i, j, WHITE);
        }
      }
    } else {                           // Divisions in between the first and last
      for (int j = 16; j < 22; ++j) {  // Adjust the height of the divisions in between (6 pixels)
        display.drawPixel(i, j, WHITE);
      }
    }
  }
  // Draw the custom bitmaps "C" and "H"
  drawBitmap(X_PADDING - 2, 2, bitmapC, 8, 10);                 // Adjust the position and size as needed
  drawBitmap(SCREEN_WIDTH - 8 - X_PADDING, 2, bitmapH, 8, 10);  // Adjust the position and size as needed

  // Draw the bitmap for the temperature sprite
  drawBitmap((SCREEN_WIDTH / 2) - 5, SCREEN_HEIGHT - 10, bitmapBottom, 10, 9);  // Adjust the position and size as needed

  // Draw the gauge bar using the sprite
  for (int i = 2; i < (currentParam == Voltage ? voltValue / (20 / GAUGE_LENGTH) : gaugeValue / (120 / GAUGE_LENGTH)); i++) {


    //gaugeValue / (120 / GAUGE_LENGTH); i++) {
    drawVerticalBar(i * BAR_WIDTH, 25, barSprite, BAR_WIDTH, BAR_HEIGHT);
  }

  // Display "(Parameter): (value)" at the top centered
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(20, 4);  // Position for top text
  switch (currentParam) {
    case ECT:
      display.print("ECT: ");
      display.print(gaugeValue);  // Display the actual ECT value here
      display.print("C");
      break;
    case OilTemp:
      display.print("Oil Temp: ");
      display.print(gaugeValue);  // Display the actual Oil Temp value here
      display.print("C");
      break;
    case Voltage:
      display.print("Volt: ");
      display.print(voltValue);  // Display the actual Voltage value here
      display.print("V");
      break;
  }
}

//Text Display Mode
void displayTextOutput(float tempValue, float voltValue) {

  display.clearDisplay();
  int tempIntValue = int(tempValue);
  // Display text for the current parameter
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 4);  // Centered position for text
  switch (currentParam) {
    case ECT:

      display.print("ECT: ");
      display.setTextSize(2);       // Larger text size
      display.setCursor(10, 25);    // Position for the larger text
      display.print(tempIntValue);  // Display the actual value here
      display.print("C");

      break;

    case OilTemp:
      display.print("Oil Temp: ");
      display.setTextSize(2);       // Larger text size
      display.setCursor(10, 25);    // Position for the larger text
      display.print(tempIntValue);  // Display the actual value here
      display.print("C");
      break;

    case Voltage:
      display.print("Voltage: ");
      display.setTextSize(2);
      display.setCursor(10, 25);
      display.print(voltValue);  // Display the actual value here
      display.print("V");
      break;
  }
}

void switchParameterForward() {
  switch (currentParam) {
    case ECT:
      currentParam = OilTemp;
      break;
    case OilTemp:
      currentParam = Voltage;
      break;
    case Voltage:
      currentParam = ECT;
      break;
    default:
      break;
  }
}

void switchParameterBackward() {
  switch (currentParam) {
    case ECT:
      currentParam = Voltage;
      break;
    case OilTemp:
      currentParam = ECT;
      break;
    case Voltage:
      currentParam = OilTemp;
      break;
    default:
      break;
  }
}


void setup() {
  Serial.begin(9600);
  while (!Serial)
    ;

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;
  }
  Serial.println(F("SSD1306 allocation success"));

//Initialise CAN with timeout
#define MAX_CAN_INIT_ATTEMPTS 10
  int canInitAttempts = 0;

  Serial.println("Initializing CAN...");
  while (canInitAttempts < MAX_CAN_INIT_ATTEMPTS) {
    if (CAN_OK == CAN.begin(CAN_125KBPS)) {
      Serial.println("CAN init successful!");
      break;  // Exit the loop if CAN initialization succeeds
    } else {
      Serial.println("CAN init fail, retrying...");
      delay(100);
      canInitAttempts++;
    }
  }

  if (canInitAttempts >= MAX_CAN_INIT_ATTEMPTS) {
    Serial.println("CAN initialization unsuccessful after maximum attempts.");
    // Add any necessary handling or continue program execution
  } else {

    //pinMode(GAUGE_PARAMETER_PIN, INPUT_PULLUP);  //Toggle Gauge parameter view
    //pinMode(DISPLAY_MODE_PIN, INPUT_PULLUP);     //Toggle Bar Graph / Text
    pinMode(buttonPin, INPUT_PULLUP);  //Toggle Bar Graph / Text

    for (int i = 0; i < epd_bitmap_allArray_LEN; ++i) {
      display.clearDisplay();
      display.drawBitmap(0, 0, epd_bitmap_allArray[i], 128, 32, WHITE);
      display.display();
      delay(2);
    }

    /*
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Initializing...");
  display.display();
  delay(500);
  display.clearDisplay();
  display.display();
  */
  }
}

float processTemperature(unsigned char buf[], int position, const char* paramName, int offset) {
  unsigned int rawValue = buf[position / 8] | (buf[(position / 8) + 1] << 8);
  float tempValue = rawValue - offset;
  Serial.print(paramName);
  Serial.print(": ");
  Serial.print(tempValue);
  Serial.println(" deg C");
  return tempValue;
}

float processVoltage(unsigned char buf[], int position, const char* paramName) {
  unsigned int rawValue = buf[position / 8] | (buf[(position / 8) + 1] << 8);
  float voltValue = rawValue / 100.0;  // Scale voltage
  Serial.print(paramName);
  Serial.print(": ");
  Serial.print(voltValue);
  Serial.println(" Volts");
  return voltValue;
}






void loop() {
  boolean CLK = digitalRead(encoderCLK);
  boolean DT = digitalRead(encoderDT);

  if (CLK != lastCLK) {
    if (DT != CLK) {
      encoderPosition++;
    } else {
      encoderPosition--;
    }
    Serial.print("Encoder Position: ");
    Serial.println(encoderPosition);
    if (encoderPosition > 0) {
      switchParameterForward();
      Serial.println("Switching to next parameter");
      encoderPosition = 0;  // Reset encoder position
    } else if (encoderPosition < 0) {
      switchParameterBackward();
      Serial.println("Switching to previous parameter");
      encoderPosition = 0;  // Reset encoder position
    }
  }
  lastCLK = CLK;

  // Read the button state with debounce
  int reading = digitalRead(buttonPin);
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;
      if (buttonState == HIGH) {
        Serial.println("Button is released");
      } else {
        displayAsBar = !displayAsBar;  // Toggle the display mode
        Serial.println("Button is pressed");
      }
    }
  }
  lastButtonState = reading;

  delay(10);  // Adjust delay as needed

  /*
  //Gauge parameter switching
  if (digitalRead(GAUGE_PARAMETER_PIN) == LOW) {
    delay(100);  // Debouncing delay
    if (digitalRead(GAUGE_PARAMETER_PIN) == LOW) {
      switchParameter();
      Serial.print("Switching to ");
      Serial.println(currentParam == ECT ? "ECT" : (currentParam == OilTemp ? "Oil Temp" : "Voltage"));
    }
    while (digitalRead(GAUGE_PARAMETER_PIN) == LOW)
      ;  // Wait for button release
  }
*/

  /*
  // Display mode switching
  if (digitalRead(DISPLAY_MODE_PIN) == LOW) {
    delay(100);  // Debouncing delay
    if (digitalRead(DISPLAY_MODE_PIN) == LOW) {
      displayAsBar = !displayAsBar;  // Toggle the display mode
      Serial.println(displayAsBar ? "Switched to Bar Graph" : "Switched to Text");
    }
    while (digitalRead(DISPLAY_MODE_PIN) == LOW)
      ;  // Wait for button release
  }
*/

  //Stream in the desired variablies in via CAN

  unsigned char len = 0;
  unsigned char buf[8];
  if (CAN_MSGAVAIL == CAN.checkReceive()) {
    CAN.readMsgBuf(&len, buf);
    switch (currentParam) {
      case ECT:
        if (buf[0] == 2) {
          tempValue = processTemperature(buf, 55, "ECT", 50);  // Store the returned temperature value
        }
        break;

      case OilTemp:
        if (buf[0] == 8) {
          tempValue = processTemperature(buf, 23, "Oil Temp", 50);
        }
        break;

      case Voltage:
        if (buf[0] == 3) {
          voltValue = processVoltage(buf, 39, "Voltage");
        }
        break;

      default:
        break;
    }

    // Display logic based on chosen display type

    if (displayAsBar) {
      displayBarGraph(tempValue, voltValue, currentParam);
    } else {
      displayTextOutput(tempValue, voltValue);
    }
    //Update the display
    display.display();
  }
}
