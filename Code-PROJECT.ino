#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

// LCD
LiquidCrystal_I2C lcd(0x27, 20, 4);

// WiFi & Firebase
#define WIFI_SSID "---"
#define WIFI_PASSWORD "-----"
#define API_KEY "---------"
#define DATABASE_URL "---------"

// Pins
#define RELAY1_PIN 27
#define RELAY2_PIN 26
#define RELAY3_PIN 25
#define RELAY4_PIN 33
#define BUTTON1_PIN 13
#define BUTTON2_PIN 12
#define BUTTON3_PIN 14
#define BUTTON4_PIN 32
#define SET_TIMER1_PIN 15
#define SET_TIMER2_PIN 2
#define SET_TIMER3_PIN 4
#define SET_TIMER4_PIN 16

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

int stateRelay1 = 0, stateRelay2 = 0, stateRelay3 = 0, stateRelay4 = 0;
int mode1 = 0, mode2 = 0, mode3 = 0, mode4 = 0;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 28800, 60000);

// Firebase paths
String relayPath1 = "/Button/SW1";
String relayPath2 = "/Button/SW2";
String relayPath3 = "/Button/SW3";
String relayPath4 = "/Button/SW4";
String modePath1 = "/Modes/Mode1";
String modePath2 = "/Modes/Mode2";
String modePath3 = "/Modes/Mode3";
String modePath4 = "/Modes/Mode4";

// Timing
unsigned long sendDataPrevMillis = 0;
unsigned long lastLCDUpdate = 0;
const unsigned long lcdInterval = 1000;
const unsigned long debounceDelay = 200;
unsigned long debounceTimers[8] = {0};  // 4 manual + 4 mode buttons

bool signupOK = false;

void setup() {
  Serial.begin(115200);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Connecting WiFi...");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  lcd.setCursor(0, 1);
  lcd.print("WiFi Connected");
  delay(1000);

  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  config.token_status_callback = tokenStatusCallback;

  if (Firebase.signUp(&config, &auth, "", "")) {
    signupOK = true;
  } else {
    Serial.printf("Firebase SignUp Failed: %s\n", config.signer.signupError.message.c_str());
  }

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  timeClient.begin();

  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  pinMode(RELAY3_PIN, OUTPUT);
  pinMode(RELAY4_PIN, OUTPUT);

  pinMode(BUTTON1_PIN, INPUT_PULLUP);
  pinMode(BUTTON2_PIN, INPUT_PULLUP);
  pinMode(BUTTON3_PIN, INPUT_PULLUP);
  pinMode(BUTTON4_PIN, INPUT_PULLUP);
  pinMode(SET_TIMER1_PIN, INPUT_PULLUP);
  pinMode(SET_TIMER2_PIN, INPUT_PULLUP);
  pinMode(SET_TIMER3_PIN, INPUT_PULLUP);
  pinMode(SET_TIMER4_PIN, INPUT_PULLUP);

  // Load saved states from Firebase
  Firebase.RTDB.getInt(&fbdo, relayPath1.c_str(), &stateRelay1);
  Firebase.RTDB.getInt(&fbdo, relayPath2.c_str(), &stateRelay2);
  Firebase.RTDB.getInt(&fbdo, relayPath3.c_str(), &stateRelay3);
  Firebase.RTDB.getInt(&fbdo, relayPath4.c_str(), &stateRelay4);

  Firebase.RTDB.getInt(&fbdo, modePath1.c_str(), &mode1);
  Firebase.RTDB.getInt(&fbdo, modePath2.c_str(), &mode2);
  Firebase.RTDB.getInt(&fbdo, modePath3.c_str(), &mode3);
  Firebase.RTDB.getInt(&fbdo, modePath4.c_str(), &mode4);

  digitalWrite(RELAY1_PIN, stateRelay1);
  digitalWrite(RELAY2_PIN, stateRelay2);
  digitalWrite(RELAY3_PIN, stateRelay3);
  digitalWrite(RELAY4_PIN, stateRelay4);

  updateLCD();
}

void loop() {
  timeClient.update();

  checkButton(BUTTON1_PIN, stateRelay1, RELAY1_PIN, relayPath1, mode1, 0);
  checkButton(BUTTON2_PIN, stateRelay2, RELAY2_PIN, relayPath2, mode2, 1);
  checkButton(BUTTON3_PIN, stateRelay3, RELAY3_PIN, relayPath3, mode3, 2);
  checkButton(BUTTON4_PIN, stateRelay4, RELAY4_PIN, relayPath4, mode4, 3);

  checkTimerMode(SET_TIMER1_PIN, mode1, modePath1, 4);
  checkTimerMode(SET_TIMER2_PIN, mode2, modePath2, 5);
  checkTimerMode(SET_TIMER3_PIN, mode3, modePath3, 6);
  checkTimerMode(SET_TIMER4_PIN, mode4, modePath4, 7);

  handleTimerMode(mode1, RELAY1_PIN, stateRelay1, relayPath1);
  handleTimerMode(mode2, RELAY2_PIN, stateRelay2, relayPath2);
  handleTimerMode(mode3, RELAY3_PIN, stateRelay3, relayPath3);
  handleTimerMode(mode4, RELAY4_PIN, stateRelay4, relayPath4);

  if (millis() - lastLCDUpdate > lcdInterval) {
    updateLCD();
    lastLCDUpdate = millis();
  }

  if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 300)) {
    sendDataPrevMillis = millis();
    Firebase.RTDB.setInt(&fbdo, relayPath1.c_str(), stateRelay1);
    Firebase.RTDB.setInt(&fbdo, relayPath2.c_str(), stateRelay2);
    Firebase.RTDB.setInt(&fbdo, relayPath3.c_str(), stateRelay3);
    Firebase.RTDB.setInt(&fbdo, relayPath4.c_str(), stateRelay4);
    Firebase.RTDB.setInt(&fbdo, modePath1.c_str(), mode1);
    Firebase.RTDB.setInt(&fbdo, modePath2.c_str(), mode2);
    Firebase.RTDB.setInt(&fbdo, modePath3.c_str(), mode3);
    Firebase.RTDB.setInt(&fbdo, modePath4.c_str(), mode4);
  }
}

void updateLCD() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("SW1:" + String(stateRelay1) + " SW2:" + String(stateRelay2));
  lcd.setCursor(0, 1);
  lcd.print("SW3:" + String(stateRelay3) + " SW4:" + String(stateRelay4));
  lcd.setCursor(0, 2);
  lcd.print("Time: " + timeClient.getFormattedTime());
  lcd.setCursor(0, 3);
  lcd.print("M1:" + String(mode1) + " M2:" + String(mode2) + " M3:" + String(mode3) + " M4:" + String(mode4));
}

void checkButton(int btnPin, int &relayState, int relayPin, String path, int mode, int index) {
  if (mode != 0) return; // Disable manual control in timer mode

  if (digitalRead(btnPin) == LOW && millis() - debounceTimers[index] > debounceDelay) {
    relayState = !relayState;
    digitalWrite(relayPin, relayState);
    Firebase.RTDB.setInt(&fbdo, path.c_str(), relayState);
    debounceTimers[index] = millis();
  }
}

void checkTimerMode(int pin, int &mode, String path, int index) {
  if (digitalRead(pin) == LOW && millis() - debounceTimers[index] > debounceDelay) {
    mode = (mode + 1) % 4;  // Cycle 0–3
    Firebase.RTDB.setInt(&fbdo, path.c_str(), mode);
    debounceTimers[index] = millis();
  }
}

void handleTimerMode(int mode, int pin, int &state, String path) {
  int hr = timeClient.getHours();
  bool shouldBeOn = false;

  if (mode == 1 && (hr >= 17 || hr < 5)) shouldBeOn = true;
  else if (mode == 2 && (hr >= 5 && hr < 12)) shouldBeOn = true;
  else if (mode == 3 && (hr >= 12 && hr < 17)) shouldBeOn = true;

  if (mode != 0) {
    if (shouldBeOn && state == 0) {
      state = 1;
      digitalWrite(pin, HIGH);
      Firebase.RTDB.setInt(&fbdo, path.c_str(), state);
    } else if (!shouldBeOn && state == 1) {
      state = 0;
      digitalWrite(pin, LOW);
      Firebase.RTDB.setInt(&fbdo, path.c_str(), state);
    }
  }
}