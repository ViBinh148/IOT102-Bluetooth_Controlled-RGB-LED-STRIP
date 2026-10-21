
#include <Adafruit_NeoPixel.h>
#include <SoftwareSerial.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <string.h>
#include <ctype.h>

// ======================================
// 1. PIN CONFIG
// ======================================

#define LED_PIN 11
#define SOUND_PIN A0
#define POT_PIN A1
#define BUTTON_PIN 2

#define BT_RX 10
#define BT_TX 9
#define BT_STATE_PIN 7
#define BUZZER_PIN 8

#define NUM_LEDS 60

// ======================================
// 2. OBJECTS
// ======================================

Adafruit_NeoPixel strip(
  NUM_LEDS,
  LED_PIN,
  NEO_GRB + NEO_KHZ800);

SoftwareSerial bluetooth(BT_RX, BT_TX);
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ======================================
// 3. MODES
// ======================================

enum LedMode {
  SOUND_MODE,
  MANUAL_MODE,
  RAINBOW_MODE
};

LedMode currentMode = SOUND_MODE;

bool ledEnabled = true;

// ======================================
// 4. SOUND SETTINGS
// ======================================


int sensitivity = 5; //độ nhạy
int noiseThreshold = 6;

int soundLevel = 0;
int ledCount = 0;

// ======================================
// 5. MANUAL COLOR SETTINGS
// ======================================

uint32_t manualColor;
const char* colorName = "RED";


bool potEnabled = false; //chiết áp mặc định là tắt

int lastPotValue = -1;
unsigned long lastPotUpdate = 0;

// ======================================
// 6. RAINBOW SETTINGS
// ======================================

uint16_t rainbowHue = 0;

unsigned long lastRainbowUpdate = 0;

// Smaller interval = faster
unsigned long rainbowInterval = 20;

// ======================================
// 7. TIMERS
// ======================================

unsigned long lastLCDUpdate = 0;
unsigned long lastLEDUpdate = 0;

// ======================================
// 8. BUTTON SETTINGS
// ======================================

bool lastButtonReading = HIGH;
bool stableButtonState = HIGH;

unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

// ======================================
// 9. BLUETOOTH CONNECTION
// ======================================

bool lastStateReading = false;
bool bluetoothConnected = false;
unsigned long lastStateChange = 0;

// Bluetooth command buffer
char btBuffer[16];
byte btLength = 0;
unsigned long lastBTCharTime = 0;

// ======================================
// 10. BUZZER
// ======================================

byte buzzerStep = 0;
unsigned long buzzerStepTime = 0;

void playConnectionSound() {
  tone(BUZZER_PIN, 1500);

  buzzerStep = 1;
  buzzerStepTime = millis();
}

void updateBuzzer() {
  unsigned long now = millis();

  if (buzzerStep == 1 &&
      now - buzzerStepTime >= 100) {

    noTone(BUZZER_PIN);

    buzzerStep = 2;
    buzzerStepTime = now;
  }

  else if (buzzerStep == 2 &&
           now - buzzerStepTime >= 80) {

    tone(BUZZER_PIN, 2000);

    buzzerStep = 3;
    buzzerStepTime = now;
  }

  else if (buzzerStep == 3 &&
           now - buzzerStepTime >= 150) {

    noTone(BUZZER_PIN);
    buzzerStep = 0;
  }
}

// ======================================
// 11. CHECK BLUETOOTH CONNECTION
// ======================================

void checkBluetoothConnection() {

  // Assumes HC-05 STATE = HIGH
  // when connected
  bool reading =
    digitalRead(BT_STATE_PIN) == HIGH;

  if (reading != lastStateReading) {
    lastStateReading = reading;
    lastStateChange = millis();
  }

  if (millis() - lastStateChange < 150) {
    return;
  }

  if (reading != bluetoothConnected) {
    bluetoothConnected = reading;

    if (bluetoothConnected) {
      Serial.println("Bluetooth Connected");
      playConnectionSound();
    }
    else {
      Serial.println("Bluetooth Disconnected");
    }
  }
}

// ======================================
// 12. MODE STATUS
// ======================================

const char* getModeName() {
  switch (currentMode) {
    case SOUND_MODE:
      return "SOUND";

    case MANUAL_MODE:
      return "MANUAL";

    case RAINBOW_MODE:
      return "RAINBOW";
  }

 

void sendModeStatus() {
  bluetooth.print("MODE:");
  bluetooth.println(getModeName());
}

// ======================================
// 13. CHANGE MODE
// ======================================

void changeMode(LedMode newMode) {
  currentMode = newMode;
  ledEnabled = true;

  soundLevel = 0;
  ledCount = 0;

  strip.clear();
  strip.show();

  Serial.print("Mode: ");
  Serial.println(getModeName());

  // Notify phone even when button is pressed
  sendModeStatus();
}

// ======================================
// 14. PHYSICAL BUTTON
// ======================================

void checkButton() {
  bool reading = digitalRead(BUTTON_PIN);

  if (reading != lastButtonReading) {
    lastDebounceTime = millis();
  }

  if (millis() - lastDebounceTime > debounceDelay) {
    if (reading != stableButtonState) {

      stableButtonState = reading;

      if (stableButtonState == LOW) {

        LedMode nextMode =
          (LedMode)((currentMode + 1) % 3);

        changeMode(nextMode);
      }
    }
  }

  lastButtonReading = reading;
}

// ======================================
// 15. SET MANUAL COLOR
// ======================================

void setManualColor(
  uint8_t r,
  uint8_t g,
  uint8_t b,
  const char* name
) {

  // Bluetooth color has priority
  potEnabled = false;

  changeMode(MANUAL_MODE);

  manualColor = strip.Color(r, g, b);
  colorName = name;

  bluetooth.print("COLOR:");
  bluetooth.println(name);

  Serial.print("Color: ");
  Serial.println(name);
}

// ======================================
// 16. PROCESS BLUETOOTH COMMAND
// ======================================

void processBluetoothCommand(const char* command) {

  Serial.print("Received: ");
  Serial.println(command);

  // ----- MODES -----

  if (strcmp(command, "S") == 0) {
    changeMode(SOUND_MODE);
  }

  else if (strcmp(command, "M") == 0) {
    changeMode(MANUAL_MODE);
  }

  else if (strcmp(command, "A") == 0) {
    changeMode(RAINBOW_MODE);
  }

  // ----- MANUAL COLORS -----

  else if (strcmp(command, "R") == 0) {
    setManualColor(255, 0, 0, "RED");
  }

  else if (strcmp(command, "G") == 0) {
    setManualColor(0, 255, 0, "GREEN");
  }

  else if (strcmp(command, "B") == 0) {
    setManualColor(0, 0, 255, "BLUE");
  }

  else if (strcmp(command, "PU") == 0) {
    setManualColor(128, 0, 255, "PURPLE");
  }

  else if (strcmp(command, "PK") == 0) {
    setManualColor(255, 105, 180, "PINK");
  }

  else if (strcmp(command, "Y") == 0) {
    setManualColor(255, 255, 0, "YELLOW");
  }

  else if (strcmp(command, "OR") == 0) {
    setManualColor(255, 128, 0, "ORANGE");
  }

  else if (strcmp(command, "CY") == 0) {
    setManualColor(0, 255, 255, "CYAN");
  }

  else if (strcmp(command, "MG") == 0) {
    setManualColor(255, 0, 255, "MAGENTA");
  }

  else if (strcmp(command, "W") == 0) {
    setManualColor(255, 255, 255, "WHITE");
  }

  else if (strcmp(command, "WW") == 0) {
    setManualColor(255, 180, 90, "WARM");
  }

  else if (strcmp(command, "BL") == 0 ||
           strcmp(command, "K") == 0) {
    setManualColor(0, 0, 0, "BLACK");
  }

  // ----- LED ON / OFF -----

  else if (strcmp(command, "O") == 0) {
    ledEnabled = false;
    bluetooth.println("LED:OFF");
  }

  else if (strcmp(command, "N") == 0) {
    ledEnabled = true;
    bluetooth.println("LED:ON");
  }

  // ----- POTENTIOMETER -----

  else if (strcmp(command, "P") == 0) {

    potEnabled = true;
    lastPotValue = -1;

    changeMode(MANUAL_MODE);

    bluetooth.println("POT:ON");
  }

  else if (strcmp(command, "Q") == 0) {

    potEnabled = false;

    bluetooth.println("POT:OFF");
  }

  // ----- RAINBOW SPEED -----

  else if (strcmp(command, "+") == 0) {

    if (rainbowInterval > 5) {
      rainbowInterval -= 5;
    }

    bluetooth.print("RAINBOW_SPEED:");
    bluetooth.println(rainbowInterval);
  }

  else if (strcmp(command, "-") == 0) {

    if (rainbowInterval < 100) {
      rainbowInterval += 5;
    }

    bluetooth.print("RAINBOW_SPEED:");
    bluetooth.println(rainbowInterval);
  }

  // ----- REQUEST CURRENT STATUS -----

  else if (strcmp(command, "?") == 0) {
    sendModeStatus();

    bluetooth.print("COLOR:");
    bluetooth.println(colorName);

    bluetooth.print("POT:");
    bluetooth.println(potEnabled ? "ON" : "OFF");

    bluetooth.print("LED:");
    bluetooth.println(ledEnabled ? "ON" : "OFF");
  }

  else {
    bluetooth.println("UNKNOWN COMMAND");
  }
}

// ======================================
// 17. READ BLUETOOTH
// ======================================

void readBluetooth() {

  while (bluetooth.available()) {
    char c = bluetooth.read();

    // End command on newline
    if (c == '\n' || c == '\r') {

      if (btLength > 0) {
        btBuffer[btLength] = '\0';

        processBluetoothCommand(btBuffer);
        btLength = 0;
      }
    }

    else if (btLength < sizeof(btBuffer) - 1) {
      btBuffer[btLength++] = c;
      lastBTCharTime = millis();
    }

    else {
      btLength = 0;
    }
  }

  // Also accept commands without newline
  if (btLength > 0 &&
      millis() - lastBTCharTime >= 150) {

    btBuffer[btLength] = '\0';

    processBluetoothCommand(btBuffer);
    btLength = 0;
  }
}

// ======================================
// 18. SOUND SENSOR
// ======================================

void readSoundSensor() {

  int minSound = 1023;
  int maxSound = 0;

  unsigned long startTime = millis();

  while (millis() - startTime < 20) {

    int value = analogRead(SOUND_PIN);

    if (value < minSound) {
      minSound = value;
    }

    if (value > maxSound) {
      maxSound = value;
    }
  }

  int amplitude = maxSound - minSound;

  amplitude = max(0, amplitude - noiseThreshold);

  int target = constrain(
    (long)amplitude * sensitivity,
    0L,
    255L
  );

  soundLevel = (soundLevel * 2 + target) / 3;

  ledCount = map(
    soundLevel, 0, 180, 0, NUM_LEDS
  );

  ledCount = constrain(
    ledCount, 0, NUM_LEDS
  );
}

// ======================================
// 19. POTENTIOMETER
// ======================================

void updatePotentiometer() {

  // Ignore A1 if pot control is disabled
  if (!potEnabled) {
    return;
  }

  if (!ledEnabled || currentMode != MANUAL_MODE) {
    return;
  }

  if (millis() - lastPotUpdate < 50) {
    return;
  }

  lastPotUpdate = millis();

  // Average 8 analog readings
  long total = 0;

  for (int i = 0; i < 8; i++) {
    total += analogRead(POT_PIN);
  }

  int potValue = total / 8;

  // Save starting position
  if (lastPotValue < 0) {

    lastPotValue = potValue;

    // Initial color when pot control starts
    uint16_t hue = map(
      potValue, 0, 1023, 0, 65535
    );

    manualColor = strip.gamma32(
      strip.ColorHSV(hue, 255, 255)
    );

    colorName = "CUSTOM";
    return;
  }

  // Filter small fluctuations
  if (abs(potValue - lastPotValue) < 15) {
    return;
  }

  lastPotValue = potValue;

  uint16_t hue = map(
    potValue, 0, 1023, 0, 65535
  );

  manualColor = strip.gamma32(
    strip.ColorHSV(hue, 255, 255)
  );

  colorName = "CUSTOM";
}

// ======================================
// 20. LED EFFECTS
// ======================================

void updateLED() {

  // LED OFF
  if (!ledEnabled) {

    strip.clear();
    strip.show();
    return;
  }

  // SOUND MODE
  if (currentMode == SOUND_MODE) {

    readSoundSensor();

    strip.clear();

    for (int i = 0; i < ledCount; i++) {

      if (i < NUM_LEDS / 3) {

        strip.setPixelColor(
          i, strip.Color(0, 255, 0)
        );
      }

      else if (i < NUM_LEDS * 2 / 3) {

        strip.setPixelColor(
          i, strip.Color(255, 150, 0)
        );
      }

      else {

        strip.setPixelColor(
          i, strip.Color(255, 0, 0)
        );
      }
    }

    strip.show();
  }

  // MANUAL MODE
  else if (currentMode == MANUAL_MODE) {

    strip.fill(manualColor);
    strip.show();
  }

  // RAINBOW MODE
  else if (currentMode == RAINBOW_MODE) {

    if (millis() - lastRainbowUpdate >=
        rainbowInterval) {

      lastRainbowUpdate = millis();

      for (int i = 0; i < NUM_LEDS; i++) {

        uint16_t hue =
          rainbowHue +
          (uint32_t)i * 65536UL / NUM_LEDS;

        strip.setPixelColor(
          i,
          strip.gamma32(strip.ColorHSV(hue))
        );
      }

      strip.show();

      rainbowHue += 320;
    }
  }
}

// ======================================
// 21. LCD DISPLAY
// ======================================

void printLCDLine(byte row, const char* message) {

  lcd.setCursor(0, row);
  lcd.print(message);

  byte length = strlen(message);

  for (byte i = length; i < 16; i++) {
    lcd.print(' ');
  }
}

void updateLCD() {

  if (millis() - lastLCDUpdate < 300) {
    return;
  }

  lastLCDUpdate = millis();

  char line1[17];
  char line2[17];

  if (!ledEnabled) {

    strcpy(line1, "LED: OFF");

    snprintf(
      line2, sizeof(line2),
      "MODE: %s",
      getModeName()
    );
  }

  else if (currentMode == SOUND_MODE) {

    strcpy(line1, "MODE: SOUND");

    snprintf(
      line2, sizeof(line2),
      "LED: %d/%d",
      ledCount,
      NUM_LEDS
    );
  }

  else if (currentMode == MANUAL_MODE) {

    strcpy(line1, "MODE: MANUAL");

    snprintf(
      line2, sizeof(line2),
      "COLOR: %s",
      colorName
    );
  }

  else {

    strcpy(line1, "MODE: RAINBOW");

    snprintf(
      line2, sizeof(line2),
      "SPEED: %lums",
      rainbowInterval
    );
  }

  printLCDLine(0, line1);
  printLCDLine(1, line2);
}

// ======================================
// 22. SETUP
// ======================================

void setup() {

  Serial.begin(9600);
  bluetooth.begin(9600);

  pinMode(SOUND_PIN, INPUT);
  pinMode(POT_PIN, INPUT);

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BT_STATE_PIN, INPUT);

  // Initialize LED
  strip.begin();
  strip.setBrightness(30);
  strip.clear();
  strip.show();

  manualColor = strip.Color(255, 0, 0);

  // Pot disabled by default
  potEnabled = false;

  // Initialize LCD
  lcd.init();
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print("IOT102 PROJECT");

  lcd.setCursor(0, 1);
  lcd.print("Starting...");

  delay(1000);
  lcd.clear();

  Serial.println("System Ready!");
  bluetooth.println("System Ready!");
}

// ======================================
// 23. MAIN LOOP
// ======================================

void loop() {

  checkBluetoothConnection();

  checkButton();

  readBluetooth();

  updatePotentiometer();

  // Sound mode runs about every 30ms
  if (currentMode != SOUND_MODE ||
      millis() - lastLEDUpdate >= 30) {

    lastLEDUpdate = millis();

    updateLED();
  }

  updateLCD();

  updateBuzzer();

  delay(5);
}
