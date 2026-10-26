/*
  ESP32 Smart Door Access System - Local Password Version
  - Blynk and Wi-Fi dependency removed
  - First-time password setup using the keypad
  - Password stored in ESP32 NVS using Preferences
  - Password reset via a physical button inside the property
  - Keypad: # = submit, * = clear, A = toggle PIR, B = lock door

  IMPORTANT:
  - Connect a normally-open reset button between GPIO 34 and GND.
  - GPIO 34 has no internal pull-up. Add an external 10k pull-up resistor
    from GPIO 34 to 3.3V.
  - The reset button should be physically inside the secured area.
  - This is a prototype, not a certified security lock.
*/

#include <Arduino.h>
#include <Keypad.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <SPI.h>
#include <ESP32Servo.h>
#include <Preferences.h>

// TFT pins (existing wiring)
#define TFT_CS    5
#define TFT_DC    2
#define TFT_RST   4

// Keypad pins (existing wiring)
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};
byte rowPins[ROWS] = {32, 33, 25, 26};
byte colPins[COLS] = {27, 14, 12, 13};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// Door hardware (existing wiring)
#define SERVO_PIN   21
#define BUZZER_PIN  19
#define LED_RED     16
#define LED_GREEN   17
#define LED_BLUE    15
#define LED_YELLOW  0
#define PIR_SENSOR  22

// GPIO 34 is input-only and has no internal pull-up.
// Use an external 10k pull-up resistor to 3.3V and button to GND.
#define RESET_BUTTON_PIN 34
const unsigned long RESET_HOLD_MS = 5000;

Adafruit_ILI9341 tft(TFT_CS, TFT_DC, TFT_RST);
Servo doorLock;
Preferences preferences;

String savedPassword = "";
String inputPassword = "";
String newPassword = "";
String confirmPassword = "";

bool passwordSetupMode = false;
bool confirmingNewPassword = false;
bool isLocked = true;
bool pirEnabled = false;
bool intruderAlertActive = false;
int attemptCount = 0;

unsigned long resetButtonDownAt = 0;
bool resetButtonHandled = false;
unsigned long lastBlinkTime = 0;
const unsigned long blinkInterval = 500;

void showMessage(const String &message) {
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 20);
  tft.println(message);
}

String maskedText(const String &value) {
  String result;
  for (size_t i = 0; i < value.length(); i++) result += '*';
  return result;
}

void showPasswordEntry(const String &heading, const String &value) {
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10, 20);
  tft.println(heading);
  tft.setCursor(10, 65);
  tft.println(maskedText(value));
  tft.setTextSize(1);
  tft.setCursor(10, 120);
  tft.println("# = Confirm");
  tft.println("* = Clear");
}

void setLockedIndicators() {
  digitalWrite(LED_RED, HIGH);
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_BLUE, LOW);
}

void lockDoor() {
  // Only move the servo if the door was actually unlocked.
  // This prevents ordinary keypad presses from moving an already-locked servo.
  if (!isLocked) {
    doorLock.write(0);
    isLocked = true;
    tone(BUZZER_PIN, 1000, 120);
  }
  setLockedIndicators();
  showMessage("Door locked");
  delay(700);
  if (passwordSetupMode) {
    showPasswordEntry(
      confirmingNewPassword ? "Confirm password:" : "Create password:",
      confirmingNewPassword ? confirmPassword : newPassword
    );
  } else {
    showMessage("Enter password:\n# to submit");
  }
}

void unlockDoor() {
  // Only move the servo after a successful password check.
  if (isLocked) {
    doorLock.write(90);
    isLocked = false;
  }
  digitalWrite(LED_RED, LOW);
  digitalWrite(LED_GREEN, HIGH);
  digitalWrite(LED_BLUE, LOW);
  tone(BUZZER_PIN, 1800, 150);
  showMessage("Door unlocked");
}

void startPasswordSetup() {
  passwordSetupMode = true;
  confirmingNewPassword = false;
  inputPassword = "";
  newPassword = "";
  confirmPassword = "";
  showPasswordEntry("Create password:", newPassword);
  Serial.println("Password setup mode started.");
}

void savePassword(const String &candidate) {
  preferences.begin("smartdoor", false);
  preferences.putString("password", candidate);
  preferences.end();

  savedPassword = candidate;
  passwordSetupMode = false;
  confirmingNewPassword = false;
  newPassword = "";
  confirmPassword = "";
  inputPassword = "";
  attemptCount = 0;

  showMessage("Password saved!\nDoor locked");
  tone(BUZZER_PIN, 1500, 150);
  delay(1800);
  lockDoor();
}

void handlePasswordSetupKey(char key) {
  String &activeEntry = confirmingNewPassword ? confirmPassword : newPassword;

  if (key == '*') {
    activeEntry = "";
    showPasswordEntry(
      confirmingNewPassword ? "Confirm password:" : "Create password:",
      activeEntry
    );
    return;
  }

  if (key == '#') {
    if (activeEntry.length() < 4 || activeEntry.length() > 8) {
      showMessage("Use 4-8 digits");
      delay(1200);
      showPasswordEntry(
        confirmingNewPassword ? "Confirm password:" : "Create password:",
        activeEntry
      );
      return;
    }

    if (!confirmingNewPassword) {
      confirmingNewPassword = true;
      confirmPassword = "";
      showPasswordEntry("Confirm password:", confirmPassword);
      return;
    }

    if (newPassword == confirmPassword) {
      savePassword(newPassword);
    } else {
      showMessage("Passwords differ\nTry again");
      tone(BUZZER_PIN, 400, 400);
      delay(1600);
      newPassword = "";
      confirmPassword = "";
      confirmingNewPassword = false;
      showPasswordEntry("Create password:", newPassword);
    }
    return;
  }

  // Only accept digits as password characters.
  if (key >= '0' && key <= '9' && activeEntry.length() < 8) {
    activeEntry += key;
    showPasswordEntry(
      confirmingNewPassword ? "Confirm password:" : "Create password:",
      activeEntry
    );
    tone(BUZZER_PIN, 500, 25);
  }
}

void alertIntruder() {
  intruderAlertActive = true;
  showMessage("Too many tries!\nAlarm active");
  for (int i = 0; i < 10; i++) {
    digitalWrite(LED_RED, HIGH);
    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_BLUE, LOW);
    tone(BUZZER_PIN, 1000, 450);
    delay(1000);
    digitalWrite(LED_RED, LOW);
  }
  noTone(BUZZER_PIN);
  intruderAlertActive = false;
  attemptCount = 0;
  lockDoor();
}

void checkPassword() {
  if (inputPassword == savedPassword && savedPassword.length() > 0) {
    attemptCount = 0;
    unlockDoor();
    delay(5000);
    lockDoor();
  } else {
    attemptCount++;
    showMessage("Wrong password");
    digitalWrite(LED_BLUE, HIGH);
    digitalWrite(LED_RED, LOW);
    tone(BUZZER_PIN, 800, 350);
    delay(1000);

    if (attemptCount >= 3) {
      alertIntruder();
    } else {
      lockDoor();
    }
  }
  inputPassword = "";
}

void togglePIR() {
  pirEnabled = !pirEnabled;
  // PIR module remains powered; this flag enables/disables software detection.
  showMessage(pirEnabled ? "Motion sensing\nENABLED" : "Motion sensing\nDISABLED");
  tone(BUZZER_PIN, pirEnabled ? 1200 : 500, 150);
  delay(1200);
  // Do not call lockDoor() here: toggling motion detection must not move the servo.
  if (passwordSetupMode) {
    showPasswordEntry("Create password:", newPassword);
  } else {
    showMessage("Enter password:\n# to submit");
  }
}

void handleResetButton() {
  bool pressed = (digitalRead(RESET_BUTTON_PIN) == LOW);

  if (pressed && resetButtonDownAt == 0) {
    resetButtonDownAt = millis();
    resetButtonHandled = false;
    showMessage("Hold reset button\n5 seconds");
  }

  if (pressed && !resetButtonHandled &&
      millis() - resetButtonDownAt >= RESET_HOLD_MS) {
    resetButtonHandled = true;

    if (isLocked) {
      startPasswordSetup();
      showMessage("Reset accepted\nSet new password");
      delay(900);
      showPasswordEntry("Create password:", newPassword);
    } else {
      showMessage("Lock door first\nthen reset");
      delay(1500);
      lockDoor();
    }
  }

  if (!pressed) {
    resetButtonDownAt = 0;
    resetButtonHandled = false;
  }
}

void detectMotion() {
  if (!pirEnabled || isLocked == false) return;

  if (digitalRead(PIR_SENSOR) == HIGH) {
    digitalWrite(LED_BLUE, HIGH);
    tone(BUZZER_PIN, 650, 180);
    showMessage("Motion detected");
    delay(500);
    showMessage("Enter password");
  }
}

void handleKeypad() {
  char key = keypad.getKey();
  if (!key) return;

  tone(BUZZER_PIN, 500, 20);

  if (passwordSetupMode) {
    handlePasswordSetupKey(key);
    return;
  }

  if (key == '*') {
    inputPassword = "";
    showPasswordEntry("Enter password:", inputPassword);
    return;
  }

  if (key == 'A') {
    togglePIR();
    return;
  }

  // B is intentionally ignored so keypad buttons cannot manually move the servo.

  if (key == '#') {
    if (inputPassword.length() == 0) {
      showMessage("Enter password\nfirst");
      delay(900);
      showPasswordEntry("Enter password:", inputPassword);
    } else {
      checkPassword();
    }
    return;
  }

  if (key >= '0' && key <= '9' && inputPassword.length() < 8) {
    inputPassword += key;
    showPasswordEntry("Enter password:", inputPassword);
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(PIR_SENSOR, INPUT);
  pinMode(RESET_BUTTON_PIN, INPUT); // External 10k pull-up required

  tft.begin();
  tft.setRotation(0);
  tft.fillScreen(ILI9341_BLACK);

  doorLock.setPeriodHertz(50);
  doorLock.attach(SERVO_PIN);
  doorLock.write(0); // Set the initial locked position at startup.
  isLocked = true;
  setLockedIndicators();

  preferences.begin("smartdoor", true);
  savedPassword = preferences.getString("password", "");
  preferences.end();

  if (savedPassword.length() < 4 || savedPassword.length() > 8) {
    savedPassword = "";
    startPasswordSetup();
  } else {
    showMessage("Smart door ready\nEnter password");
  }

  Serial.println("Smart Door Access System started in local mode.");
}

void loop() {
  handleResetButton();
  handleKeypad();
  detectMotion();

  if (millis() - lastBlinkTime >= blinkInterval) {
    lastBlinkTime = millis();
    digitalWrite(LED_YELLOW, !digitalRead(LED_YELLOW));
  }
}
