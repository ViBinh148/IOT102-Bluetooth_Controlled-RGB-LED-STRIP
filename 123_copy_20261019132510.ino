//các thư viện cần thiết
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
#define BT_RX 10
#define BT_TX 9
#define NUM_LEDS 60

//=====================================
// 2. OBJECT CONFIG
// ======================================
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
SoftwareSerial bluetooth(BT_RX, BT_TX);


// ======================================
// 3. MODES
// ======================================

enum LedMode {
  SOUND_MODE,
  MANUAL_MODE
};

LedMode currentMode = SOUND_MODE; // Mặc định chế độ là SOUND_MODE

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
const char* colorName = "RED"; // Màu mặc định là đỏ


bool potEnabled = false; //chiết áp mặc định là tắt

int lastPotValue = -1; //chiết áp mặc định là -1 để không tự động cập nhật màu khi khởi động
unsigned long lastPotUpdate = 0;

// ======================================
// 6. TIMERS //bộ đếm thời gian
// ======================================

unsigned long lastLCDUpdate = 0;
unsigned long lastLEDUpdate = 0;

// ======================================
// 7. BLUETOOTH CONNECTION
// ======================================

bool lastStateReading = false;
bool bluetoothConnected = false;
unsigned long lastStateChange = 0;

//đây là bộ đệm để lưu trữ dữ liệu nhận được từ Bluetooth
char btBuffer[16];
byte btLength = 0;
unsigned long lastBTCharTime = 0;

// ======================================
// 8. CHECK BLUETOOTH CONNECTION
// ======================================

void checkBluetoothConnection() {

  // Kiểm tra trạng thái của chân BT_STATE_PIN để xác định xem Bluetooth có đang kết nối hay không
  bool reading =
    digitalRead(BT_STATE_PIN) == HIGH;

  if (reading != lastStateReading) {
    lastStateReading = reading;
    lastStateChange = millis(); // Cập nhật thời gian thay đổi trạng thái
  }

  if (millis() - lastStateChange < 150) { // Nếu trạng thái thay đổi trong vòng 150ms, bỏ qua để tránh nhiễu
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
// 9. MODE STATUS
// ======================================

const char* getModeName() {
  switch (currentMode) {
    case SOUND_MODE:
      return "SOUND";

    case MANUAL_MODE:
      return "MANUAL";
    default:
      return "UNKNOWN";
  }
}

void sendModeStatus() { //gửi qua bluetooth
  bluetooth.print("MODE:");
  bluetooth.println(getModeName());
}

// ======================================
// 10. CHANGE MODE
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
// 11. SET MANUAL COLOR
// ======================================

void setManualColor(
  uint8_t r,
  uint8_t g,
  uint8_t b,
  const char* name
) {

  // Tắt chế độ chiết áp khi người dùng chọn màu thủ công
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
// 12. PROCESS BLUETOOTH COMMAND
// ======================================
//dự kiến các manual color
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

  // ----- Chiết áp -----

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

  // ======================================
// 13. READ BLUETOOTH
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

  // đọc dữ liệu từ Bluetooth và xử lý khi không nhận được ký tự mới trong 150ms
  if (btLength > 0 &&
      millis() - lastBTCharTime >= 150) {

    btBuffer[btLength] = '\0';

    processBluetoothCommand(btBuffer);
    btLength = 0;
  }
}

// ======================================
// 14. SOUND SENSOR
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
