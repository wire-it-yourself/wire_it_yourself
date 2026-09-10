/***************************************************************************
 *  Pomodoro Timer Cube
 *  ---------------------------------------------------------------------
 *  An Arduino-based Pomodoro timer housed in a cube. Rotate the cube to
 *  choose a duration (25 / 10 / 5 minutes), press the button to start,
 *  stop or reset, and the display rotates to stay upright. When time is
 *  up, the buzzer plays a melody and the display flashes.
 *
 *  Hardware
 *    - Arduino (ATmega328P-class board, e.g. Uno / Nano)
 *    - 128x64 SSD1306 OLED display over I2C (address 0x3C)
 *    - 3x LDR (light-dependent resistors) on A0, A1, A2 for orientation
 *    - Push button on D9 (INPUT_PULLUP, active low)
 *    - Passive buzzer on D11
 *
 *  Controls
 *    - Single press : start / pause the timer
 *    - Double press : reset the timer to the selected duration
 *
 *  Libraries (install via Arduino Library Manager)
 *    - Adafruit GFX Library      https://github.com/adafruit/Adafruit-GFX-Library
 *    - Adafruit SSD1306          https://github.com/adafruit/Adafruit_SSD1306
 *    SPI and Wire are part of the Arduino core.
 *
 *  Author  : Shweta Mane
 *  Source  : https://github.com/manesl/wire_it_yourself
 *  License : MIT (see LICENSE in the repository root)
 *
 *  Copyright (c) 2026 Shweta Mane
 *
 *  Permission is hereby granted, free of charge, to any person obtaining
 *  a copy of this software and associated documentation files (the
 *  "Software"), to deal in the Software without restriction, including
 *  without limitation the rights to use, copy, modify, merge, publish,
 *  distribute, sublicense, and/or sell copies of the Software, and to
 *  permit persons to whom the Software is furnished to do so, subject to
 *  the following conditions:
 *
 *  The above copyright notice and this permission notice shall be
 *  included in all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 *  OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 *  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 *  CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 *  TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 *  SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 ***************************************************************************/
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define CUBE_DISPLAY_TEXT_SIZE 2

#define CUBE_25_MIN_ORIENT  0 // cube is sitting with bottom LDR covered, showing 25:00
#define CUBE_25_MIN_ROTATION 0
#define CUBE_25_MIN_DEFAULT_TEXT "25:00"

#define CUBE_5_MIN_ORIENT   1 // cube is sitting with right LDR covered, showing 05:00
#define CUBE_5_MIN_ROTATION 3
#define CUBE_5_MIN_DEFAULT_TEXT "05:00"

#define CUBE_10_MIN_ORIENT  2 // cube is sitting with left LDR covered, showing 10:00
#define CUBE_10_MIN_ROTATION 1
#define CUBE_10_MIN_DEFAULT_TEXT "10:00"

#define CUBE_INVALID_ORIENT -1 // invalid orientation, show 00:00
#define CUBE_INVALID_ROTATION 0
#define CUBE_INVALID_DEFAULT_TEXT "00:00"

#define POMODORO_BUTTON_PIN 9
#define NO_PRESS 0
#define SINGLE_PRESS 1
#define DOUBLE_PRESS 2
#define CONFIDENCE_THRESHOLD 3 // Average was 2.82; 3 ignores most noise
#define WAIT_FOR_SECOND_PRESS_WINDOW 350 // ms to wait for a second press

/* Buzzer definitions */
#define BUZZER_PIN 11
#define NOTE_B4  494

// Static variables to track the timer state and orientation
static int32_t timerStartValue = 0;
static int32_t remainingSeconds = 0;
static int8_t lastOrientation = CUBE_INVALID_ORIENT;
static uint16_t lastDisplayedSeconds = 0xFFFF; // Initialize with a value the timer will never hit

// Global Variables
int consecutivePressCount = 0;
int buttonPressCount = 0;
unsigned long lastPressTime = 0;
bool singlePressPending = false;
bool buttonPressed = false;

/* Buzzer variables */
int frequency = NOTE_B4;

/* Tomato bitmap*/
const unsigned char tomato_bmp [] PROGMEM = {
	0xff, 0xff, 0xff, 0xf8, 0x1f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf7, 0xe7, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xe7, 0xe7, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xe0, 0x07, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xe1, 0xef, 0xf7, 0xff, 0xff, 0xff, 0xff, 0xff, 0xe0, 0x37, 0xf4, 0x0f, 0xff, 0xff, 
	0xff, 0xff, 0x87, 0x87, 0xe0, 0xe7, 0xff, 0xff, 0xff, 0xf8, 0x07, 0xf7, 0xf7, 0xe0, 0x7f, 0xff, 
	0xff, 0xf3, 0xfb, 0xff, 0xff, 0xe0, 0x0f, 0xff, 0xff, 0xcf, 0xf9, 0xff, 0xff, 0xdf, 0xe3, 0xff, 
	0xff, 0x9f, 0xfc, 0xff, 0xc7, 0x9f, 0xf9, 0xff, 0xff, 0x3f, 0xfe, 0x7f, 0xcf, 0x3f, 0xfc, 0x7f, 
	0xfe, 0x7f, 0x3f, 0x3f, 0x9c, 0xff, 0xff, 0x3f, 0xfc, 0xfc, 0x3f, 0x3f, 0xfc, 0xff, 0xff, 0xbf, 
	0xfd, 0xf8, 0x7f, 0x7f, 0x3e, 0xff, 0xff, 0xdf, 0xfb, 0xe4, 0xfe, 0x7e, 0x1e, 0x7f, 0xff, 0xcf, 
	0xf3, 0xc9, 0xfe, 0xf8, 0xcf, 0x7f, 0xff, 0xe7, 0xf7, 0x93, 0xfe, 0xc3, 0xe3, 0x3f, 0xff, 0xf7, 
	0xe7, 0xb6, 0x3c, 0x0f, 0xf8, 0x3f, 0xff, 0xf3, 0xef, 0x24, 0x3e, 0x7f, 0xff, 0xff, 0xff, 0xfb, 
	0xce, 0x6c, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xf9, 0xde, 0xca, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfd, 
	0xdc, 0xd0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfd, 0xbd, 0x95, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfd, 
	0xbd, 0xb5, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfc, 0xb9, 0xab, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 
	0x3b, 0xab, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x7b, 0x2b, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 
	0x73, 0x63, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x77, 0x77, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 
	0x77, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x77, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 
	0x77, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x67, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 
	0x67, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x67, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 
	0x67, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x67, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 
	0x76, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x70, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 
	0x79, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfc, 0x3c, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfd, 
	0xb8, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfd, 0xb8, 0xf8, 0xff, 0xff, 0xff, 0xfe, 0xff, 0xfd, 
	0xbd, 0xf8, 0x7f, 0xff, 0xff, 0xfc, 0x7f, 0xf9, 0x9e, 0x70, 0x7f, 0xff, 0xff, 0xfa, 0x3f, 0xfb, 
	0xde, 0x70, 0x7f, 0xff, 0xe7, 0xf8, 0x3f, 0xf3, 0xcf, 0xf0, 0x7f, 0xf7, 0xe7, 0xf8, 0x3f, 0xf7, 
	0xef, 0xf0, 0x7f, 0xf3, 0x87, 0xf8, 0x3f, 0xe7, 0xe7, 0xf0, 0x7f, 0xe0, 0x07, 0xf8, 0x3f, 0xef, 
	0xf7, 0xf8, 0x7f, 0xe7, 0xff, 0xf8, 0x3f, 0xcf, 0xf3, 0xfc, 0xff, 0xff, 0xff, 0xfc, 0x7f, 0xdf, 
	0xf9, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xbf, 0xfd, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x3f, 
	0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x7f, 0xff, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xfc, 0xff, 
	0xff, 0xbf, 0xff, 0xff, 0xff, 0xff, 0xf9, 0xff, 0xff, 0xcf, 0xff, 0xff, 0xff, 0xff, 0xf3, 0xff, 
	0xff, 0xe7, 0xff, 0xff, 0xff, 0xff, 0xcf, 0xff, 0xff, 0xf1, 0xff, 0xff, 0xff, 0xff, 0x1f, 0xff, 
	0xff, 0xfc, 0x7f, 0xff, 0xff, 0xfc, 0x7f, 0xff, 0xff, 0xff, 0x0f, 0xff, 0xff, 0xe1, 0xff, 0xff, 
	0xff, 0xff, 0xe0, 0xff, 0xfe, 0x07, 0xff, 0xff, 0xff, 0xff, 0xfc, 0x00, 0x00, 0x7f, 0xff, 0xff
};

static void drawCenteredText(const char *text, uint8_t textSize) {
  display.clearDisplay();
  display.setTextSize(textSize);
  display.setTextColor(SSD1306_WHITE);
  display.setTextWrap(false); // This tells to not put the text on the next line if
  // it is hitting the edge of the display. Instead, it will be clipped. 
  //This is important for centering.

  int16_t x1, y1;
  uint16_t w, h;
  // This function calculates the bounding box of the text. It returns the upper-left corner 
  // (x1, y1) and the width and height (w, h) of the box that would enclose the text.
  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);

  // Note: x1/y1 can be negative depending on font metrics.
  int16_t x = (display.width() - (int16_t)w) / 2 - x1; // centering the text horizontally
  int16_t y = (display.height() - (int16_t)h) / 2 - y1; // centering the text vertically

  display.setCursor(x, y);
  display.print(text);
  display.display();
}

static int8_t getOrientationOfCube() {
  // When the LDR is not covered, with ambient light the resitance of LDR is 8 KOhm, the Vout = 2.67 volts which 
  // roughly turns out to be 500+
  // When the LDR is covered, with darkness the resistance of LDR is 28 KOhm, the Vout = 1.3 volts, which roughly
  // turns out to be <200
  int center = analogRead(A2);
  int right = analogRead(A1);
  int left = analogRead(A0);
  int8_t orient = CUBE_INVALID_ORIENT;
  // Serial.print(F("center: "));
  // Serial.println(center);
  // Serial.print(F("right: "));
  // Serial.println(right);
  // Serial.print(F("left: "));
  // Serial.println(left);

  // These values work for my desktop lamp and ambient light. You may need to adjust the thresholds based on your environment.
  if (center < 300 && right > 250 && left > 300) {
    orient = CUBE_25_MIN_ORIENT; // show 25:00
  } else if (center > 500 && right < 300 && left > 500) {
    orient = CUBE_5_MIN_ORIENT; // show 5:00
  } else if (center > 350 && right > 500 && left < 300) {
    orient = CUBE_10_MIN_ORIENT; // show 10:00
  }

  //Serial.println(F("orientation: " + String(orient)));
  return orient;
}

static void setupOrientation(int8_t orientation) {
  // Reset the last displayed seconds so that the display will update immediately with the new orientation's time
  lastDisplayedSeconds = 0xFFFF; // Initialize with a value the timer will never hit

  if (orientation == CUBE_25_MIN_ORIENT) {
    display.setRotation(CUBE_25_MIN_ROTATION);
    drawCenteredText(CUBE_25_MIN_DEFAULT_TEXT, CUBE_DISPLAY_TEXT_SIZE);
    // Set the global vars timerStartValue and lastOrientation
    timerStartValue = 25 * 60; // 25 minutes in seconds, show 25:00
    lastOrientation = CUBE_25_MIN_ORIENT;
  } else if (orientation == CUBE_5_MIN_ORIENT) {
    display.setRotation(CUBE_5_MIN_ROTATION);
    drawCenteredText(CUBE_5_MIN_DEFAULT_TEXT, CUBE_DISPLAY_TEXT_SIZE);
    timerStartValue = 5 * 60; // 5 minutes in seconds, show 5:00
    lastOrientation = CUBE_5_MIN_ORIENT;
  } else if (orientation == CUBE_10_MIN_ORIENT) {
    display.setRotation(CUBE_10_MIN_ROTATION);
    drawCenteredText(CUBE_10_MIN_DEFAULT_TEXT, CUBE_DISPLAY_TEXT_SIZE);
    timerStartValue = 10 * 60; // 10 minutes in seconds, show 10:00
    lastOrientation = CUBE_10_MIN_ORIENT;
  } else {
    display.setRotation(CUBE_INVALID_ROTATION);
    drawCenteredText(CUBE_INVALID_DEFAULT_TEXT, CUBE_DISPLAY_TEXT_SIZE);
    timerStartValue = 0; // invalid orientation, show 0:00
    lastOrientation = CUBE_INVALID_ORIENT;
  }
}

static void buttonResetLogic() {
  consecutivePressCount = 0;
  lastPressTime = 0;
  buttonPressCount = 0;
  singlePressPending = false;
  buttonPressed = false;
}

static uint8_t getButtonPressType() {
  // This function is not used in the current code, but it can be implemented to return:
  // 0 for no press, 1 for single press, and 2 for double press based on the button press logic.
  // Read button (Invert if using INPUT_PULLUP: LOW = Pressed)
  bool currentState = (digitalRead(POMODORO_BUTTON_PIN) == LOW);

  // --- STEP 1: DEBOUNCE LOGIC (The Consecutive Count) ---
  if (currentState) {
    consecutivePressCount++;
  } else {
    consecutivePressCount = 0;
    buttonPressed = false; // Reset so we can detect the next physical press
  }

  // Detect a valid "New" press
  if (consecutivePressCount >= CONFIDENCE_THRESHOLD && !buttonPressed) {
    buttonPressed = true; 
    buttonPressCount++;
    lastPressTime = millis();
    Serial.print(F("Button Press Detected. Count: "));
    Serial.print(buttonPressCount);
    Serial.print(F(" at "));
    Serial.print(lastPressTime);
    Serial.println(F("ms"));
    singlePressPending = true;

    // If we hit 2 presses, trigger double-press immediately
    if (buttonPressCount == DOUBLE_PRESS) {
      // Wait until the button is released to avoid multiple triggers from the same double press
      while(digitalRead(POMODORO_BUTTON_PIN) == LOW) { 
        delay(10);
      }
      buttonResetLogic(); // reset the logic to be ready for the next press sequence
      return DOUBLE_PRESS; // double press
    }
  }

  // --- STEP 2: TIMING WINDOW LOGIC ---
  if (singlePressPending && (millis() - lastPressTime > WAIT_FOR_SECOND_PRESS_WINDOW)) {
    // If time ran out and we only have 1 press, it's a single press
    Serial.print(F("Click window expired. Press count: "));
    Serial.print(buttonPressCount);
    Serial.print(F(" at "));
    Serial.print(millis());
    Serial.print(F("ms (last press at "));
    Serial.print(lastPressTime);
    Serial.println(F("ms)"));
    if (buttonPressCount == SINGLE_PRESS) {
      buttonResetLogic(); // reset the logic to be ready for the next press sequence
      return SINGLE_PRESS; // single press
    }
  }

  return NO_PRESS;
}

static void displayTime(uint16_t remainingSeconds, bool forceUpdate = false) {
  if (remainingSeconds == lastDisplayedSeconds && !forceUpdate) {
    return; // No need to update the display if the time hasn't changed
  }

  lastDisplayedSeconds = remainingSeconds;

  uint16_t minutes = (uint16_t)(remainingSeconds / 60);
  uint8_t seconds = (uint8_t)(remainingSeconds % 60);

  char buf[6];
  snprintf(buf, sizeof(buf), "%02u:%02u", minutes, seconds);
  drawCenteredText(buf, CUBE_DISPLAY_TEXT_SIZE);
}

/* Make Timer up sound */
static void timerUpSound(void) {
  // First Click
  tone(BUZZER_PIN, frequency);
  delay(60);             // Short duration for a "click"
  noTone(BUZZER_PIN);    // Stop the sound

  // The "Gap" (Crucial to prevent bleeding)
  delay(100);             // Silence period

  // Second Click
  tone(BUZZER_PIN, frequency);
  delay(60);             // Short duration
  noTone(BUZZER_PIN);    // Stop the sound
}

void setup() {
    // buzzer setup
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, HIGH); // Set buzzer pin to HIGH (off for active LOW)

  Serial.begin(115200);

  pinMode(POMODORO_BUTTON_PIN, INPUT_PULLUP);

  // Wait for display
  delay(500);

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  // The display initialization function just like we had a sequence for the
  // hitachi lcd 1602.
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ; // Don't proceed, loop forever
  }

  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
  display.display();
  // todo: show cute tomato image here for a few seconds before showing the timer options?
  delay(2000); // Pause for 2 seconds

  // Clear the buffer
  display.clearDisplay();
  display.drawBitmap(32, 0, tomato_bmp, 64, 64, WHITE);
  delay(2000);

  // Check orientation
  int8_t currentOrientation = getOrientationOfCube();
  setupOrientation(currentOrientation);

  if(currentOrientation == CUBE_INVALID_ORIENT) {
      remainingSeconds = timerStartValue;
    }
    else {
      remainingSeconds = timerStartValue; // reset the remaining seconds to the start value of the new orientation
    }

  delay(1500); // Pause for 1.5 seconds
}

void loop() {
  static uint32_t lastTickMs = 0;
  static bool timerRunning = false; // toggle this value on each button press to start/stop the timer

  // Check orientation: What if the user has flipped the cube?
  // Need a varaible to track the current orientation, and only update the display if the orientation has changed.
  int8_t currentOrientation = getOrientationOfCube();
  if (currentOrientation != lastOrientation) {
    Serial.print(F("orientation: "));
    Serial.print(lastOrientation);
    Serial.print(F(" >> "));
    Serial.println(currentOrientation);
    setupOrientation(currentOrientation);

    if(currentOrientation == CUBE_INVALID_ORIENT) {
      remainingSeconds = timerStartValue; // 0
      timerRunning = false; // stop the timer if the cube is in an invalid orientation
    }
    else {
      remainingSeconds = timerStartValue; // reset the remaining seconds to the start value of the new orientation
      timerRunning = false; // stop the timer when orientation changes, user has to press the button to start it again
    }
  }

  uint8_t buttonPressType = getButtonPressType();
  if (buttonPressType == SINGLE_PRESS) { // single press
    Serial.println(F("Action: SINGLE PRESS"));
    if (timerRunning) {
      timerRunning = false; // stop the timer
    } else {
      Serial.println(F("timerStartValue:"));
      Serial.println(timerStartValue);
      timerRunning = true; // start the timer
      lastTickMs = millis(); // reset the tick timer so we don't lose a second immediately after starting
    }
  } else if (buttonPressType == DOUBLE_PRESS) { // double press
    Serial.println(F("Action: DOUBLE PRESS"));
    remainingSeconds = timerStartValue; // reset the timer to the start value
    timerRunning = false; // stop the timer
    // Update the display immediately to show the reset time
    uint16_t minutes = (uint16_t)(remainingSeconds / 60);
    uint8_t seconds = (uint8_t)(remainingSeconds % 60);
    displayTime(remainingSeconds, true); // force update the display to show the reset time immediately
  }

  if (!timerRunning) {
    return; // skip the rest of the loop, timer won't run
  }

  uint32_t now = millis();
  if (now - lastTickMs >= 1000) {
    lastTickMs = now;
    Serial.println(F("lastTickMs:"));
    Serial.println(lastTickMs);
    
    if (remainingSeconds > 0) {
      remainingSeconds--;
    }

    Serial.println(F("remainingSeconds:"));
    Serial.println(remainingSeconds);

    uint16_t minutes = (uint16_t)(remainingSeconds / 60); // how many full sets of 60?
    uint8_t seconds = (uint8_t)(remainingSeconds % 60); // how many seconds did not make it to that full set?

    displayTime(remainingSeconds, false); // update the display with the new remaining time

    // once the timer is up, play a sound and reset the timer to the start value but keep it stopped until the user presses the button again to start it
    if (remainingSeconds == 0) {
      timerUpSound();
      // blink the 00:00 a few times to indicate time's up
      for (int i = 0; i < 2; i++) {
        display.clearDisplay();
        display.display();
        delay(300);
        displayTime(remainingSeconds, true); // force update the display to show the time immediately
        delay(300);
      }

      // reset the timer
      remainingSeconds = timerStartValue; // reset to the start value
      displayTime(remainingSeconds, false);
      timerRunning = false; // stop the timer until user starts it again
    }
  }
}