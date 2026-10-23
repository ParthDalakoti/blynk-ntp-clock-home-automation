#define BLYNK_TEMPLATE_ID "TMPL3_Mxfznzt" 
#define BLYNK_TEMPLATE_NAME "IR HOME AUTOMATION" 
#define BLYNK_FIRMWARE_VERSION "0.6.6" 
 
#include <Wire.h> 
#include <LiquidCrystal_I2C.h> 
#include <WiFi.h> 
#include <BlynkEdgent.h> 
#include <IRremote.hpp> 
#include <DHT.h> 
#include <time.h> 
#include <EEPROM.h> 
 
// Blynk virtual pins 
#define VP_LED_RELAY    V4 
#define VP_BRD_RELAY    V5 
#define VP_MOTOR        V6 
#define VP_POWER        V8 
#define VP_ALL_OFF      V0 
#define VP_TEMP         V10 
#define VP_PASSWORD     V11 
#define VP_NEW_PASSWORD V13 
#define VP_HUMIDITY     V14 
 
// Hardware pins 
#define SDA_PIN         14 
#define SCL_PIN         27 
#define IR_RECEIVE_PIN  5 
#define GREEN_LED       16 
#define YELLOW_LED      17 
#define RED_LED         18 
#define BUZZER          21 
#define RELAY_LED       4 
#define RELAY_BRD       26 
#define MOTOR_PIN       23 
#define IR_SENSOR_PIN   32 
#define DHT_PIN         13 
#define DHTTYPE         DHT11 
 
// Global objects 
LiquidCrystal_I2C lcd(0x27, 16, 2); 
DHT dht(DHT_PIN, DHTTYPE); 
 

// NTP settings 
const char* ntpServer = "pool.ntp.org"; 
const long gmtOffset_sec = 19800; 
const int daylightOffset_sec = 0; 
 
// State variables 
bool systemPowered = false; 
bool ledRelayState = false; 
bool brdRelayState = false; 
bool motorState = false; 
bool lastIRSensorState = HIGH; 
bool passwordVerified = false; 
 
// Auto-off for LED relay 
unsigned long lastLedToggleTime = 0; 
const unsigned long AUTO_OFF_TIMEOUT = 180000; // 3 minutes 
 
// Timing variables 
unsigned long lastIRTriggerTime = 0; 
const unsigned long IR_DEBOUNCE = 200; 
unsigned long lastIRReceiveTime = 0; 
const unsigned long IR_RECEIVE_DEBOUNCE = 200; 
unsigned long msgStartTime = 0; 
const unsigned long MSG_DURATION = 5000; 
bool showingMsg = false; 
unsigned long lastMenuUpdate = 0; 
const unsigned long MENU_INTERVAL = 1000; 
unsigned long greenLedStartTime = 0; 
bool greenLedOn = false; 
unsigned long buzzerStartTime = 0; 
bool buzzerOn = false; 
unsigned long passwordMsgStartTime = 0; 
bool showingPasswordMsg = false; 
unsigned long lastIRPasswordTime = 0; 
const unsigned long IR_PASSWORD_DEBOUNCE = 300; 
 
// Startup screen states 
enum StartupScreen { START_SYSTEM, WIFI_CONNECT, WIFI_STATUS, 
ENTER_PASSWORD, RUNNING }; 
StartupScreen currentScreen = START_SYSTEM; 
unsigned long screenStartTime = 0; 
const unsigned long SCREEN_DELAY = 1500; 
 
// Password handling 
#define PASSWORD_LENGTH 4 
char password[PASSWORD_LENGTH] = {'2', '0', '0', '5'}; 
char enteredPassword[PASSWORD_LENGTH] = {0}; 
byte pwdIndex = 0; 

 
// IR command codes 
#define IR_0     81 
#define IR_1     94 
#define IR_2     6 
#define IR_3     5 
#define IR_4     71 
#define IR_5     7 
#define IR_6     64 
#define IR_7     24 
#define IR_8     68 
#define IR_9     15 
#define IR_OK    89 
#define IR_BACK  18 
#define IR_A     10 
#define IR_B     11 
#define IR_C     12 
#define IR_D     13 
#define IR_E     14 
#define IR_F     15 
#define IR_POWER 9 
 
// EEPROM settings 
#define EEPROM_SIZE 5 
#define EEPROM_ADDR 0 
#define EEPROM_INIT_FLAG_ADDR 4 
 
// Function prototypes 
void showMenu(); 
void updateTemperatureDisplay(float temp, float humidity); 
void beepBuzzer(); 
void blinkGreen(); 
void allOff(); 
void powerOn(); 
void powerOff(); 
void showMessage(const char* msg); 
void showPasswordMessage(const char* msg); 
void showStartupScreen(); 
void handlePasswordEntry(char digit); 
void handleBack(); 
void handleVerify(); 
void updatePasswordLCD(); 
void handleIRForPassword(int cmd); 
void checkWiFiStatus(); 
String getShortTime(); 
String getShortDate(); 
void loadPasswordFromEEPROM(); 
void savePasswordToEEPROM(const char* newPassword); 

bool isValidPassword(const String& input); 
 
// Blynk handlers 
BLYNK_WRITE(VP_LED_RELAY) { 
  if (!systemPowered || !passwordVerified) return; 
  ledRelayState = param.asInt(); 
  digitalWrite(RELAY_LED, ledRelayState); 
  blinkGreen(); 
  beepBuzzer(); 
  Blynk.virtualWrite(VP_LED_RELAY, ledRelayState); 
  showMessage(ledRelayState ? "LED ON" : "LED OFF"); 
  lastLedToggleTime = millis(); 
} 
 
BLYNK_WRITE(VP_BRD_RELAY) { 
  if (!systemPowered || !passwordVerified) return; 
  brdRelayState = param.asInt(); 
  digitalWrite(RELAY_BRD, brdRelayState); 
  blinkGreen(); 
  beepBuzzer(); 
  Blynk.virtualWrite(VP_BRD_RELAY, brdRelayState); 
  showMessage(brdRelayState ? "BRD ON" : "BRD OFF"); 
} 
 
BLYNK_WRITE(VP_MOTOR) { 
  if (!systemPowered || !passwordVerified) return; 
  motorState = param.asInt(); 
  digitalWrite(MOTOR_PIN, motorState ? HIGH : LOW); 
  blinkGreen(); 
  beepBuzzer(); 
  Blynk.virtualWrite(VP_MOTOR, motorState); 
  showMessage(motorState ? "MOT ON" : "MOT OFF"); 
} 
 
BLYNK_WRITE(VP_POWER) { 
  if (param.asInt() == 1) { 
    if (!systemPowered) powerOn(); 
  } else { 
    if (systemPowered) powerOff(); 
  } 
  beepBuzzer(); 
} 
 
BLYNK_WRITE(VP_ALL_OFF) { 
  if (!systemPowered || !passwordVerified) return; 
  if (param.asInt() == 1) { 
    allOff(); 
    blinkGreen(); 

    beepBuzzer(); 
    showMessage("ALL OFF"); 
  } 
} 
 
BLYNK_WRITE(VP_PASSWORD) { 
  if (passwordVerified) return; 
  String input = param.asStr(); 
  Serial.print("Password input: "); Serial.println(input); 
  if (input.length() == PASSWORD_LENGTH) { 
    passwordVerified = true; 
    for (byte i = 0; i < PASSWORD_LENGTH; i++) { 
      if (input[i] != password[i]) { 
        passwordVerified = false; 
        break; 
      } 
    } 
    if (passwordVerified) { 
      showPasswordMessage("PASSWORD OK"); 
      systemPowered = true; 
      allOff(); 
      currentScreen = RUNNING; 
      showMenu(); 
      Blynk.virtualWrite(VP_POWER, 1); 
    } else { 
      showPasswordMessage("WRONG PASSWORD"); 
    } 
  } else { 
    Serial.println("Invalid password length"); 
  } 
} 
 
BLYNK_WRITE(VP_NEW_PASSWORD) { 
  if (!systemPowered || !passwordVerified) { 
    Blynk.virtualWrite(VP_NEW_PASSWORD, "System locked!"); 
    showMessage("SYSTEM LOCKED"); 
    return; 
  } 
  String input = param.asStr(); 
  if (isValidPassword(input)) { 
    savePasswordToEEPROM(input.c_str()); 
    for (byte i = 0; i < PASSWORD_LENGTH; i++) { 
      password[i] = input[i]; 
    } 
    showPasswordMessage("PASSWORD CHANGED"); 
    Blynk.setProperty(VP_NEW_PASSWORD, "value", ""); 
  } else { 
    showPasswordMessage("INVALID PASSWORD"); 

    Blynk.virtualWrite(VP_NEW_PASSWORD, "Use 4 chars (A-Z, 0-9)!"); 
    Blynk.setProperty(VP_NEW_PASSWORD, "value", ""); 
  } 
} 
 
void setup() { 
  Serial.begin(115200); 
  delay(100); 
 
  Wire.begin(SDA_PIN, SCL_PIN); 
  lcd.init(); 
  lcd.backlight(); 
  showStartupScreen(); 
 
  EEPROM.begin(EEPROM_SIZE); 
  loadPasswordFromEEPROM(); 
 
  pinMode(GREEN_LED, OUTPUT); 
  pinMode(YELLOW_LED, OUTPUT); 
  pinMode(RED_LED, OUTPUT); 
  pinMode(BUZZER, OUTPUT); 
  pinMode(RELAY_LED, OUTPUT); 
  pinMode(RELAY_BRD, OUTPUT); 
  pinMode(MOTOR_PIN, OUTPUT); 
  pinMode(IR_SENSOR_PIN, INPUT); 
 
  dht.begin(); 
  IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK); 
  BlynkEdgent.begin(); 
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer); 
  allOff(); 
} 
 
void loop() { 
  BlynkEdgent.run(); 
  unsigned long now = millis(); 
 
  if (greenLedOn && now - greenLedStartTime > 100) { 
    digitalWrite(GREEN_LED, LOW); 
    greenLedOn = false; 
  } 
  if (buzzerOn && now - buzzerStartTime > 100) { 
    digitalWrite(BUZZER, LOW); 
    buzzerOn = false; 
  } 
 
  if (showingPasswordMsg && now - passwordMsgStartTime > MSG_DURATION) { 
    showingPasswordMsg = false; 

    if (passwordVerified) { 
      showMenu(); 
    } else { 
      lcd.clear(); 
      lcd.print("ENTER PASSWORD"); 
      updatePasswordLCD(); 
    } 
  } 
 
  if (!passwordVerified) { 
    if (currentScreen == WIFI_STATUS) { 
      checkWiFiStatus(); 
      currentScreen = ENTER_PASSWORD; 
      screenStartTime = now; 
      showStartupScreen(); 
    } else if (currentScreen != ENTER_PASSWORD && now - screenStartTime > 
SCREEN_DELAY) { 
      currentScreen = static_cast<StartupScreen>(currentScreen + 1); 
      screenStartTime = now; 
      showStartupScreen(); 
    } 
 
    if (currentScreen == ENTER_PASSWORD && IrReceiver.decode()) { 
      int cmd = IrReceiver.decodedIRData.command; 
      handleIRForPassword(cmd); 
      IrReceiver.resume(); 
    } 
    return; 
  } 
 
  currentScreen = RUNNING; 
 
  if (systemPowered && ledRelayState && now - lastLedToggleTime > 
AUTO_OFF_TIMEOUT) { 
    ledRelayState = false; 
    digitalWrite(RELAY_LED, LOW); 
    Blynk.virtualWrite(VP_LED_RELAY, 0); 
    showMessage("LED AUTO OFF"); 
    blinkGreen(); 
    beepBuzzer(); 
    Serial.println("LED relay off: 3-min inactivity"); 
  } 
 
  if (IrReceiver.decode()) { 
    int cmd = IrReceiver.decodedIRData.command; 
    Serial.print("IR Cmd: "); Serial.println(cmd); 
    if (now - lastIRReceiveTime > IR_RECEIVE_DEBOUNCE) { 
      switch (cmd) { 

        case IR_4: 
          if (systemPowered) { 
            allOff(); 
            blinkGreen(); 
            beepBuzzer(); 
            showMessage("ALL OFF"); 
            Blynk.virtualWrite(VP_ALL_OFF, 1); 
          } 
          break; 
        case IR_1: 
          if (systemPowered) { 
            ledRelayState = !ledRelayState; 
            digitalWrite(RELAY_LED, ledRelayState); 
            blinkGreen(); 
            beepBuzzer(); 
            Blynk.virtualWrite(VP_LED_RELAY, ledRelayState); 
            showMessage(ledRelayState ? "LED ON" : "LED OFF"); 
            lastLedToggleTime = millis(); 
          } 
          break; 
        case IR_2: 
          if (systemPowered) { 
            brdRelayState = !brdRelayState; 
            digitalWrite(RELAY_BRD, brdRelayState); 
            blinkGreen(); 
            beepBuzzer(); 
            Blynk.virtualWrite(VP_BRD_RELAY, brdRelayState); 
            showMessage(brdRelayState ? "BRD ON" : "BRD OFF"); 
          } 
          break; 
        case IR_3: 
          if (systemPowered) { 
            motorState = !motorState; 
            digitalWrite(MOTOR_PIN, motorState ? HIGH : LOW); 
            blinkGreen(); 
            beepBuzzer(); 
            Blynk.virtualWrite(VP_MOTOR, motorState); 
            showMessage(motorState ? "MOT ON" : "MOT OFF"); 
          } 
          break; 
        case IR_POWER: 
          if (systemPowered) powerOff(); 
          else powerOn(); 
          beepBuzzer(); 
          break; 
      } 
      lastIRReceiveTime = now; 
    } 

    IrReceiver.resume(); 
  } 
 
  if (systemPowered) { 
    bool currIR = digitalRead(IR_SENSOR_PIN); 
    if (currIR == LOW && lastIRSensorState == HIGH && now - lastIRTriggerTime > 
IR_DEBOUNCE) { 
      ledRelayState = !ledRelayState; 
      digitalWrite(RELAY_LED, ledRelayState); 
      blinkGreen(); 
      beepBuzzer(); 
      Blynk.virtualWrite(VP_LED_RELAY, ledRelayState); 
      showMessage(ledRelayState ? "LED ON" : "LED OFF"); 
      lastLedToggleTime = millis(); 
      lastIRTriggerTime = now; 
    } 
    lastIRSensorState = currIR; 
  } 
 
  static float tempC = NAN; 
  static float humidity = NAN; 
  static unsigned long lastTempRead = 0; 
  const unsigned long TEMP_INTERVAL = 2000; 
 
  if (systemPowered && now - lastTempRead > TEMP_INTERVAL) { 
    float t = dht.readTemperature(); 
    float h = dht.readHumidity(); 
    if (!isnan(t) && !isnan(h)) { 
      tempC = t; 
      humidity = h; 
      Blynk.virtualWrite(VP_TEMP, tempC); 
      Blynk.virtualWrite(VP_HUMIDITY, humidity); 
    } 
    lastTempRead = now; 
  } 
 
  if (showingMsg && now - msgStartTime > MSG_DURATION) { 
    showingMsg = false; 
    if (systemPowered) { 
      lastMenuUpdate = now; 
      showMenu(); 
    } else { 
      showMessage("SYSTEM OFF"); 
    } 
  } 
 
  if (systemPowered && !showingMsg && !showingPasswordMsg && now - lastMenuUpdate 
> MENU_INTERVAL) { 

    showMenu(); 
    if (!isnan(tempC) && !isnan(humidity)) { 
      updateTemperatureDisplay(tempC, humidity); 
    } 
    lastMenuUpdate = now; 
  } 
} 
 
void allOff() { 
  ledRelayState = brdRelayState = motorState = false; 
  digitalWrite(RELAY_LED, LOW); 
  digitalWrite(RELAY_BRD, LOW); 
  digitalWrite(MOTOR_PIN, LOW); 
  Blynk.virtualWrite(VP_LED_RELAY, 0); 
  Blynk.virtualWrite(VP_BRD_RELAY, 0); 
  Blynk.virtualWrite(VP_MOTOR, 0); 
  lastLedToggleTime = millis(); 
} 
 
void showMenu() { 
  String currentTime = getShortTime(); 
  String currentDate = getShortDate(); 
  lcd.clear(); 
  lcd.setCursor(0, 0); 
  lcd.print(currentTime); 
  lcd.print(" "); 
  lcd.print(currentDate); 
} 
 
void updateTemperatureDisplay(float temp, float humidity) { 
  lcd.setCursor(0, 1); 
  int tempInt = (int)temp; 
  int humidInt = (int)humidity; 
  char displayStr[17]; 
  snprintf(displayStr, sizeof(displayStr), "TE:%2d C HU:%2d %%", tempInt, humidInt); 
  lcd.print(displayStr); 
} 
 
void blinkGreen() { 
  if (!greenLedOn) { 
    digitalWrite(GREEN_LED, HIGH); 
    greenLedStartTime = millis(); 
    greenLedOn = true; 
  } 
} 
 
void beepBuzzer() { 
  if (!buzzerOn) { 

    digitalWrite(BUZZER, HIGH); 
    buzzerStartTime = millis(); 
    buzzerOn = true; 
  } 
} 
 
void showMessage(const char* msg) { 
  lcd.clear(); 
  lcd.setCursor(0, 0); 
  lcd.print(msg); 
  showingMsg = true; 
  msgStartTime = millis(); 
} 
 
void showPasswordMessage(const char* msg) { 
  lcd.clear(); 
  lcd.setCursor(0, 0); 
  lcd.print(msg); 
  showingPasswordMsg = true; 
  passwordMsgStartTime = millis(); 
} 
 
void powerOff() { 
  systemPowered = false; 
  allOff(); 
  digitalWrite(RED_LED, HIGH); 
  digitalWrite(YELLOW_LED, LOW); 
  showMessage("SYSTEM OFF"); 
  Blynk.virtualWrite(VP_POWER, 0); 
  lastLedToggleTime = millis(); 
} 
 
void powerOn() { 
  systemPowered = true; 
  digitalWrite(RED_LED, LOW); 
  digitalWrite(YELLOW_LED, HIGH); 
  showMessage("SYSTEM ON"); 
  Blynk.virtualWrite(VP_POWER, 1); 
} 
 
void showStartupScreen() { 
  lcd.clear(); 
  switch (currentScreen) { 
    case START_SYSTEM: lcd.print("SYSTEM STARTING"); break; 
    case WIFI_CONNECT: lcd.print("WIFI CONNECTING"); break; 
    case WIFI_STATUS: lcd.print("WIFI CONNECTED"); break; 
    case ENTER_PASSWORD: lcd.print("ENTER PASSWORD"); updatePasswordLCD(); 
break; 

  } 
} 
 
void handlePasswordEntry(char digit) { 
  if (pwdIndex < PASSWORD_LENGTH) { 
    enteredPassword[pwdIndex++] = digit; 
    updatePasswordLCD(); 
  } 
} 
 
void handleBack() { 
  if (pwdIndex > 0) { 
    pwdIndex--; 
    enteredPassword[pwdIndex] = 0; 
    updatePasswordLCD(); 
  } 
} 
 
void handleVerify() { 
  passwordVerified = true; 
  for (byte i = 0; i < PASSWORD_LENGTH; i++) { 
    if (enteredPassword[i] != password[i]) { 
      passwordVerified = false; 
      break; 
    } 
  } 
  if (passwordVerified) { 
    showPasswordMessage("PASSWORD OK"); 
    systemPowered = true; 
    allOff(); 
    currentScreen = RUNNING; 
  } else { 
    showPasswordMessage("WRONG PASSWORD"); 
    pwdIndex = 0; 
    memset(enteredPassword, 0, PASSWORD_LENGTH); 
  } 
} 
 
void updatePasswordLCD() { 
  lcd.setCursor(0, 1); 
  lcd.print("                "); 
  lcd.setCursor(0, 1); 
  for (byte i = 0; i < pwdIndex; i++) lcd.print("*"); 
} 
 
void handleIRForPassword(int cmd) { 
  unsigned long now = millis(); 
  if (now - lastIRPasswordTime < IR_PASSWORD_DEBOUNCE) return; 

  if (cmd == IR_0) handlePasswordEntry('0'); 
  else if (cmd == IR_1) handlePasswordEntry('1'); 
  else if (cmd == IR_2) handlePasswordEntry('2'); 
  else if (cmd == IR_3) handlePasswordEntry('3'); 
  else if (cmd == IR_4) handlePasswordEntry('4'); 
  else if (cmd == IR_5) handlePasswordEntry('5'); 
  else if (cmd == IR_6) handlePasswordEntry('6'); 
  else if (cmd == IR_7) handlePasswordEntry('7'); 
  else if (cmd == IR_8) handlePasswordEntry('8'); 
  else if (cmd == IR_9) handlePasswordEntry('9'); 
  else if (cmd == IR_A) handlePasswordEntry('A'); 
  else if (cmd == IR_B) handlePasswordEntry('B'); 
  else if (cmd == IR_C) handlePasswordEntry('C'); 
  else if (cmd == IR_D) handlePasswordEntry('D'); 
  else if (cmd == IR_E) handlePasswordEntry('E'); 
  else if (cmd == IR_F) handlePasswordEntry('F'); 
  else if (cmd == IR_BACK) handleBack(); 
  else if (cmd == IR_OK) handleVerify(); 
  lastIRPasswordTime = now; 
} 
 
void checkWiFiStatus() { 
  if (WiFi.status() == WL_CONNECTED) { 
    lcd.clear(); 
    lcd.print("WIFI CONNECTED"); 
  } else { 
    lcd.clear(); 
    lcd.print("WIFI FAILED"); 
    BlynkEdgent.begin(); 
  } 
} 
 
String getShortTime() { 
  struct tm timeinfo; 
  if (!getLocalTime(&timeinfo)) return "12:00 --"; 
  char timeStr[9]; 
  strftime(timeStr, sizeof(timeStr), "%I:%M %p", &timeinfo); 
  return String(timeStr); 
} 
 
String getShortDate() { 
  struct tm timeinfo; 
  if (!getLocalTime(&timeinfo)) return "00.00"; 
  char dateStr[6]; 
  strftime(dateStr, sizeof(dateStr), "%d.%m", &timeinfo); 
  return String(dateStr); 
} 
 

void loadPasswordFromEEPROM() { 
  if (EEPROM.read(EEPROM_INIT_FLAG_ADDR) != 0xFF) { 
    for (byte i = 0; i < PASSWORD_LENGTH; i++) { 
      password[i] = EEPROM.read(EEPROM_ADDR + i); 
    } 
  } else { 
    const char* defaultPassword = "2005"; 
    savePasswordToEEPROM(defaultPassword); 
    for (byte i = 0; i < PASSWORD_LENGTH; i++) { 
      password[i] = defaultPassword[i]; 
    } 
    EEPROM.write(EEPROM_INIT_FLAG_ADDR, 0x00); 
    EEPROM.commit(); 
  } 
} 
 
void savePasswordToEEPROM(const char* newPassword) { 
  for (byte i = 0; i < PASSWORD_LENGTH; i++) { 
    EEPROM.write(EEPROM_ADDR + i, newPassword[i]); 
  } 
  EEPROM.commit(); 
} 
 
bool isValidPassword(const String& input) { 
  if (input.length() != PASSWORD_LENGTH) return false; 
  for (byte i = 0; i < PASSWORD_LENGTH; i++) { 
    char c = input[i]; 
    if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z'))) return false; 
  } 
  return true; 
}