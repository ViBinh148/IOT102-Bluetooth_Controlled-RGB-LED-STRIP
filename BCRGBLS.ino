#include <Adafruit_NeoPixel.h>
#include <SoftwareSerial.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <string.h>
#include <ctype.h>

// CÁC CHÂN ĐANG CẮM TRÊN ARDUINO
#define LED_PIN 11
#define SOUND_PIN A0
#define POT_PIN A1
#define BUTTON_PIN 2
#define BUZZER_PIN 8
#define BT_STATE_PIN 7
#define LED_COUNT 60

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
SoftwareSerial bluetooth(10, 9); // D10 nhận từ HC-05 TXD, D9 gửi qua mạch chia áp đến RXD
LiquidCrystal_I2C lcd(0x27, 16, 2);

// 0 = nhảy theo nhạc; 1 = chọn màu; 2 = cầu vồng
int mode = 0;
bool ledOn = true;
bool potOn = false; // Ban đầu chỉ điều khiển màu bằng điện thoại

int soundPower = 5;
int noiseLimit = 6;
int soundLevel = 0;
int ledCount = 0;
uint32_t myColor;
const char* colorName = "RED"; //màu mặc định là đỏ

unsigned long rainbowSpeed = 20; // Số càng nhỏ thì màu chạy càng nhanh
uint16_t rainbowColor = 0;
unsigned long lastRainbow = 0;
unsigned long lastPotTime = 0;
unsigned long lastLcdTime = 0;
unsigned long lastLedTime = 0;
int lastPot = -1;

// Nhớ trạng thái nút để mỗi lần nhấn chỉ đổi một chế độ
bool lastButton = HIGH;
bool buttonState = HIGH;
unsigned long lastButtonTime = 0;

// Nhớ trạng thái kết nối Bluetooth
bool lastBtRead = LOW;
bool btConnected = false;
unsigned long lastBtTime = 0;

// Nhớ các chữ do điện thoại gửi (ví dụ: PU, BL, WW)
char command[16];
byte commandSize = 0;
unsigned long lastCharTime = 0;

// Hai tiếng bíp khi điện thoại kết nối
byte beepStep = 0;
unsigned long lastBeepTime = 0;

// LẤY TÊN CHẾ ĐỘ
const char* getMode() {
  if (mode == 0) return "SOUND";
  if (mode == 1) return "MANUAL";
  return "RAINBOW";
}

// GỬI CHẾ ĐỘ HIỆN TẠI VỀ ĐIỆN THOẠI
void sendMode() {
  bluetooth.print("MODE:");
  bluetooth.println(getMode());
}

// CHUYỂN CHẾ ĐỘ
void setMode(int newMode) {
  mode = newMode;
  potOn = false; // Chuyen che do se ngung dieu khien bang chiet ap
  ledOn = true;
  soundLevel = 0;
  ledCount = 0;
  strip.clear();
  strip.show();
  sendMode();
}

// ĐỔI MÀU BẰNG ĐIỆN THOẠI
void setColor(int red, int green, int blue, const char* name) {
  potOn = false; // Không để chiết áp tự đổi màu đã chọn trên điện thoại
  setMode(1);
  myColor = strip.Color(red, green, blue);
  colorName = name;
  bluetooth.print("COLOR:");
  bluetooth.println(colorName);
}

// XỬ LÝ LỆNH ĐIỆN THOẠI GỬI XUỐNG
void doCommand(const char* text) {
  switch (strlen(text)) {
    case 1: // LỆNH MỘT CHỮ
      switch (text[0]) {
        case 'S': setMode(0); break;
        case 'M': setMode(1); break;
        case 'A': setMode(2); break;
        case 'R': setColor(255, 0, 0, "RED"); break;
        case 'G': setColor(0, 255, 0, "GREEN"); break;
        case 'B': setColor(0, 0, 255, "BLUE"); break;
        case 'Y': setColor(255, 255, 0, "YELLOW"); break;
        case 'W': setColor(255, 255, 255, "WHITE"); break;
        case 'K': setColor(0, 0, 0, "BLACK"); break;
        case 'O':
          ledOn = false;
          bluetooth.println("LED:OFF");
          break;
        case 'N':
          ledOn = true;
          bluetooth.println("LED:ON");
          break;
        case 'P':
          setMode(1);
          potOn = true;  // Bật chiết áp để chọn màu bằng P trên điện thoại
          lastPot = -1;
          bluetooth.println("POT:ON");
          break;
        case 'Q':
          potOn = false;
          bluetooth.println("POT:OFF");
          break;
        case '+':
          if (rainbowSpeed > 5) rainbowSpeed -= 5;
          bluetooth.print("RAINBOW_SPEED:");
          bluetooth.println(rainbowSpeed);
          break;
        case '-':
          if (rainbowSpeed < 100) rainbowSpeed += 5;
          bluetooth.print("RAINBOW_SPEED:");
          bluetooth.println(rainbowSpeed);
          break;
        case '?':
          sendMode();
          bluetooth.print("COLOR:"); bluetooth.println(colorName);
          bluetooth.print("POT:"); bluetooth.println(potOn ? "ON" : "OFF");
          bluetooth.print("LED:"); bluetooth.println(ledOn ? "ON" : "OFF");
          break;
        default:
          bluetooth.println("UNKNOWN COMMAND");
          break;
      }
      break;

    case 2: // LỆNH HAI CHỮ
      switch (text[0]) {
        case 'P':
          switch (text[1]) {
            case 'U': setColor(128, 0, 255, "PURPLE"); break;
            case 'K': setColor(255, 105, 180, "PINK"); break;
            default: bluetooth.println("UNKNOWN COMMAND"); break;
          }
          break;
        case 'O':
          switch (text[1]) {
            case 'R': setColor(255, 128, 0, "ORANGE"); break;
            default: bluetooth.println("UNKNOWN COMMAND"); break;
          }
          break;
        case 'C':
          switch (text[1]) {
            case 'Y': setColor(0, 255, 255, "CYAN"); break;
            default: bluetooth.println("UNKNOWN COMMAND"); break;
          }
          break;
        case 'M':
          switch (text[1]) {
            case 'G': setColor(255, 0, 255, "MAGENTA"); break;
            default: bluetooth.println("UNKNOWN COMMAND"); break;
          }
          break;
        case 'W':
          switch (text[1]) {
            case 'W': setColor(255, 180, 90, "WARM"); break;
            default: bluetooth.println("UNKNOWN COMMAND"); break;
          }
          break;
        case 'B':
          switch (text[1]) {
            case 'L': setColor(0, 0, 0, "BLACK"); break;
            default: bluetooth.println("UNKNOWN COMMAND"); break;
          }
          break;
        default:
          bluetooth.println("UNKNOWN COMMAND");
          break;
      }
      break;

    default:
      bluetooth.println("UNKNOWN COMMAND");
      break;
  }
}

// NHẬN LỆNH BLUETOOTH, CẢ LỆNH HAI CHỮ
void readBluetooth() {
  while (bluetooth.available()) {
    char letter = bluetooth.read();
    if (letter == '\n' || letter == '\r') {
      if (commandSize > 0) {
        command[commandSize] = '\0';
        doCommand(command);
        commandSize = 0;
      }
    }
    else if (commandSize < sizeof(command) - 1) {
      command[commandSize++] = toupper((unsigned char)letter);
      lastCharTime = millis();
    }
    else commandSize = 0;
  }

  // nếu quá 150ms mà không có ký tự mới thì coi như lệnh đã xong
  if (commandSize > 0 && millis() - lastCharTime >= 150) {
    command[commandSize] = '\0';
    doCommand(command);
    commandSize = 0;
  }
}

// NHẤN NÚT D2 ĐỂ CHUYỂN BA CHẾ ĐỘ
void readButton() {
  bool button = digitalRead(BUTTON_PIN);

  if (button != lastButton) lastButtonTime = millis(); // Lưu thời điểm thay đổi trạng thái nút

  if (millis() - lastButtonTime > 50 && button != buttonState) {
    buttonState = button;
    if (buttonState == LOW) setMode((mode + 1) % 3);
  }
  lastButton = button;
}

// PHÁT HAI TIẾNG BÍP
void startBeep() {
  tone(BUZZER_PIN, 1500);
  beepStep = 1;
  lastBeepTime = millis();
}

void playBeep() {
  unsigned long now = millis();
  if (beepStep == 1 && now - lastBeepTime >= 100) {
    noTone(BUZZER_PIN);
    beepStep = 2;
    lastBeepTime = now;
  }
  else if (beepStep == 2 && now - lastBeepTime >= 80) {
    tone(BUZZER_PIN, 2000);
    beepStep = 3;
    lastBeepTime = now;
  }
  else if (beepStep == 3 && now - lastBeepTime >= 150) {
    noTone(BUZZER_PIN);
    beepStep = 0;
  }
}

// KIỂM TRA ĐIỆN THOẠI VỪA KẾT NỐI BLUETOOTH
void checkBluetooth() {
  // Cần nối chân STATE của HC-05 vào D7 (HIGH khi đã kết nối)
  bool connected = digitalRead(BT_STATE_PIN) == HIGH;
  if (connected != lastBtRead) {
    lastBtRead = connected;
    lastBtTime = millis();
  }
  if (millis() - lastBtTime < 150) return;
  if (connected != btConnected) {
    btConnected = connected;
    if (btConnected) startBeep();
  }
}

// ĐỌC CHIẾT ÁP A1 ĐỂ CHỌN MÀU
void readPot() {
  if (!potOn || !ledOn || mode != 1) return;
  if (millis() - lastPotTime < 50) return;
  lastPotTime = millis();

  long sum = 0;
  for (int i = 0; i < 8; i++) sum += analogRead(POT_PIN);
  int value = sum / 8;
  if (lastPot >= 0 && abs(value - lastPot) < 15) return;
  lastPot = value;

  uint16_t hue = map(value, 0, 1023, 0, 65535);
  myColor = strip.gamma32(strip.ColorHSV(hue));
  colorName = "CUSTOM";

}

// ĐỌC ÂM THANH VÀ TÍNH SỐ LED CẦN SÁNG
void readSound() {
  int minValue = 1023;
  int maxValue = 0;
  unsigned long start = millis();

  while (millis() - start < 20) {
    int value = analogRead(SOUND_PIN);
    if (value < minValue) minValue = value;
    if (value > maxValue) maxValue = value;
  }

  int sound = max(0, maxValue - minValue - noiseLimit);
  int newLevel = constrain((long)sound * soundPower, 0L, 255L);
  soundLevel = (soundLevel * 2 + newLevel) / 3;
  ledCount = constrain(map(soundLevel, 0, 180, 0, LED_COUNT), 0, LED_COUNT);
}

// HIỂN THỊ LED THEO CHẾ ĐỘ HIỆN TẠI
void showLed() {
  if (!ledOn) {
    strip.clear();
    strip.show();
  }
  else if (mode == 0) {
    readSound();
    strip.clear();
    for (int i = 0; i < ledCount; i++) {
      if (i < LED_COUNT / 3) strip.setPixelColor(i, strip.Color(0, 255, 0));
      else if (i < LED_COUNT * 2 / 3) strip.setPixelColor(i, strip.Color(255, 150, 0));
      else strip.setPixelColor(i, strip.Color(255, 0, 0));
    }
    strip.show();
  }
  else if (mode == 1) {
    strip.fill(myColor);
    strip.show();
  }
  else if (millis() - lastRainbow >= rainbowSpeed) {
    lastRainbow = millis();
    for (int i = 0; i < LED_COUNT; i++) {
      uint16_t color = rainbowColor + (uint32_t)i * 65536UL / LED_COUNT;
      strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(color)));
    }
    strip.show();
    rainbowColor += 320;
  }
}

// IN MỘT DÒNG LÊN MÀN HÌNH LCD
void printLine(byte row, const char* message) {
  lcd.setCursor(0, row);
  lcd.print(message);
  for (byte i = strlen(message); i < 16; i++) lcd.print(' ');
}

// HIỂN THỊ CHẾ ĐỘ, MÀU HOẶC SỐ LED ĐANG SÁNG
void showLcd() {
  if (millis() - lastLcdTime < 300) return;
  lastLcdTime = millis();

  char line1[17];
  char line2[17];
  if (!ledOn) {
    strcpy(line1, "LED: OFF");
    snprintf(line2, sizeof(line2), "MODE: %s", getMode());
  }
  else if (mode == 0) {
    strcpy(line1, "MODE: SOUND");
    snprintf(line2, sizeof(line2), "LED: %d/%d", ledCount, LED_COUNT);
  }
  else if (mode == 1) {
    strcpy(line1, "MODE: MANUAL");
    snprintf(line2, sizeof(line2), "COLOR: %s", colorName);
  }
  else {
    strcpy(line1, "MODE: RAINBOW");
    snprintf(line2, sizeof(line2), "SPEED: %lums", rainbowSpeed);
  }
  printLine(0, line1);
  printLine(1, line2);
}

// CHẠY MỘT LẦN KHI ARDUINO ĐƯỢC BẬT
void setup() {
  Serial.begin(9600);
  bluetooth.begin(9600);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(BT_STATE_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(SOUND_PIN, INPUT);
  pinMode(POT_PIN, INPUT);

  strip.begin();
  strip.setBrightness(30);
  strip.clear();
  strip.show();
  myColor = strip.Color(255, 0, 0);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Group 2 Xin Chao!");
  lcd.setCursor(0, 1);
  lcd.print("Starting...");
  delay(1000);
  lcd.clear();
}

// LẶP LẠI LIÊN TỤC
void loop() {
  checkBluetooth();
  readButton();
  readBluetooth();
  readPot();

  if (mode != 0 || millis() - lastLedTime >= 30) {
    lastLedTime = millis();
    showLed();
  }

  showLcd();
  playBeep();
  delay(5);
}
