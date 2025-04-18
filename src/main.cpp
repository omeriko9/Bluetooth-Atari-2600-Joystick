#include <Arduino.h>
#include <BleGamepad.h>
#include <NimBLEDevice.h>
#include <esp_sleep.h>
#include <AXP20X.h>
#include <M5StickCPlus.h> 

AXP20X_Class axp;

// --- Device Info ---
#define DEVICE_NAME "ESP32 Joystick"
#define MANUFACTURER_NAME "Omer Agmon"

#define PIN_BATTERY_ADC 35
#define BUZZER_PIN 2

// GPIO Pin Assignments
#define PIN_UP 26
#define PIN_DOWN 36
#define PIN_LEFT 0
#define PIN_RIGHT 32
#define PIN_FIRE 33

// --- BLE Setup ---
BleGamepadConfiguration bleConfig;
BleGamepad gamepad(DEVICE_NAME, MANUFACTURER_NAME, 100, false);

// --- Timing ---
unsigned long lastClickTime = 0;
bool buttonPressed = false;
unsigned long fireLeftPressedStartTime = 0;

// Timings (milliseconds)
#define DEBOUNCE_DELAY 50                   // ms - Reduce noise from button presses
#define PAIRING_MODE_HOLD_TIME 5000         // ms - Hold FIRE button for this long to enter pairing mode
#define BATTERY_UPDATE_INTERVAL (60 * 1000) // ms - Update battery level every 1 minute
#define RESTART_HOLD_TIME 6000              // ms
#define INACTIVITY_TIMEOUT_MS (60 * 1000)   // 1 minute = 60 000 ms
#define SLEEP_POLL_US 100000                // how often we poll the hat‐directions (in microseconds) - 100ms

// Input states
bool currentStateUp = false;
bool currentStateDown = false;
bool currentStateLeft = false;
bool currentStateRight = false;
bool currentStateFire = false;

bool lastStateUp = false;
bool lastStateDown = false;
bool lastStateLeft = false;
bool lastStateRight = false;
bool lastStateFire = false;

// Timing & State variables
unsigned long lastActivityTime = 0;
unsigned long lastDebounceTimeFire = 0;
unsigned long fireButtonPressedStartTime = 0;
bool fireButtonHeld = false;
bool isInPairingMode = false;
unsigned long lastBatteryUpdateTime = 0;
byte currentBatteryLevel = 100;
bool wasConnected = false;

float getBatteryVoltage()
{
  // 1) read volts from the PMIC
  float v = M5.Axp.GetBatVoltage();
  //    └── returns battery voltage in volts :contentReference[oaicite:1]{index=1}

  // 2) map 3.0 V→0% and 4.2 V→100%
  const float V_MIN = 3.0f;
  const float V_MAX = 4.2f;
  float pct = (v - V_MIN) / (V_MAX - V_MIN);
  if (pct < 0)
    pct = 0;
  if (pct > 1)
    pct = 1;

  // 3) scale to 0–100
  return (uint8_t)(pct * 100.0f);
}

void readInputsAndSendReport()
{
  bool up = (digitalRead(PIN_UP) == LOW);
  bool down = (digitalRead(PIN_DOWN) == LOW);
  bool left = (digitalRead(PIN_LEFT) == LOW);
  bool right = (digitalRead(PIN_RIGHT) == LOW);
  bool fire = (digitalRead(PIN_FIRE) == LOW);

  if (up != lastStateUp ||
      down != lastStateDown ||
      left != lastStateLeft ||
      right != lastStateRight ||
      fire != lastStateFire)
  {

    lastStateUp = up;
    lastStateDown = down;
    lastStateLeft = left;
    lastStateRight = right;
    lastStateFire = fire;

    if (gamepad.isConnected())
    {
      uint8_t hat = HAT_CENTERED;

      if (up && left)
        hat = HAT_UP_LEFT;
      else if (up && right)
        hat = HAT_UP_RIGHT;
      else if (down && left)
        hat = HAT_DOWN_LEFT;
      else if (down && right)
        hat = HAT_DOWN_RIGHT;
      else if (up)
        hat = HAT_UP;
      else if (down)
        hat = HAT_DOWN;
      else if (left)
        hat = HAT_LEFT;
      else if (right)
        hat = HAT_RIGHT;

      gamepad.setHat1(hat);

      lastActivityTime = millis();

      if (fire)
        gamepad.press(BUTTON_1);
      else
        gamepad.release(BUTTON_1);
    }
  }
}

void armDeepSleep_FireOnly()
{
  // EXT0: wake when FIRE goes LOW
  esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_FIRE, 0);
  esp_deep_sleep_start();
}

bool anyDirPressed()
{
  return (digitalRead(PIN_UP) == LOW) || (digitalRead(PIN_DOWN) == LOW) || (digitalRead(PIN_LEFT) == LOW) || (digitalRead(PIN_RIGHT) == LOW);
}

void PreventSleepOnBattery()
{
  // Prevent AXP192 from turning off CPU
  axp.setPowerOutPut(AXP192_DCDC1, AXP202_ON); // ESP32
  axp.setPowerOutPut(AXP192_LDO2, AXP202_ON);  // Display (if still connected)
  axp.setPowerOutPut(AXP192_LDO3, AXP202_ON);  // RTC (optional)

  // Optional: boost battery charging current
  // axp.setChargeControlCur(AXP202_CHARGE_CUR_100MA);

  // Optional: screen backlight off (if display removed)
  axp.setDCDC3Voltage(0);
}

void bleBeep()
{
  for (int i = 800; i < 1400; i += 50)
  {
    M5.Beep.tone(i, 200);
    delay(100);        
    M5.Beep.mute();
  }
}

void wakeupBeep()
{
  for (int i = 600; i < 1500; i += 50)
  {
    M5.Beep.tone(i, 50);
    delay(20);        
    M5.Beep.mute();
  }

}

void setup()
{

  esp_sleep_wakeup_cause_t reason = esp_sleep_get_wakeup_cause();

  // Configure GPIOs
  pinMode(PIN_UP, INPUT_PULLUP);
  pinMode(PIN_DOWN, INPUT);
  pinMode(PIN_LEFT, INPUT_PULLUP);
  pinMode(PIN_RIGHT, INPUT_PULLUP);
  pinMode(PIN_FIRE, INPUT_PULLUP);

  if (reason == ESP_SLEEP_WAKEUP_TIMER)
  {
    // woken by our poll timer — did _any_ direction actually get pressed?
    if (!anyDirPressed())
    {
      // nope -> go right back to sleep
      armDeepSleep_FireOnly();
      // never returns…
    }
    // else: at least one direction is down → fall through to normal startup
    wakeupBeep(); // signal wake by direction
  }
  else if (reason == ESP_SLEEP_WAKEUP_EXT0)
  {
    // woken by FIRE → normal startup
    wakeupBeep();
  }
  else
  {
    // first boot or other wake → your existing power‑on beep
    M5.Beep.tone(800, 500);
    delay(150);
    M5.Beep.mute();
  }

  M5.begin();
  M5.Beep.begin();
  Serial.begin(115200);
  delay(1000); // Wait for Serial to connect
  Serial.println("M5StickCPlus is alive (no screen).");

  // Buzzer
  M5.Beep.tone(800, 500);
  delay(150);
  M5.Beep.mute();

  // Configure BLE Gamepad
  Serial.println("Configure gamepad...");
  bleConfig.setControllerType(CONTROLLER_TYPE_JOYSTICK);
  bleConfig.setVid(0x0810);
  bleConfig.setPid(0x0001);
  Serial.println("gamepad begin...");
  gamepad.begin(&bleConfig); // This initializes NimBLE and starts advertising
  delay(1000);
  Serial.print("BLE MAC Address: ");
  Serial.println(NimBLEDevice::getAddress().toString().c_str());

  Serial.println("BLE Joystick initialized, waiting for connection...");

  gamepad.setBatteryLevel(getBatteryVoltage());

  // After initializing BLE
  BLEAdvertising *advertising = BLEDevice::getAdvertising();
  advertising->setMinInterval(0x00A0); // Min adv interval (0x0020–0x4000), units = 0.625ms
  advertising->setMaxInterval(0x00F0);

  // esp_sleep_enable_timer_wakeup(10 * 1000);  // 10 ms

  lastActivityTime = millis();
}

void loop()
{
  unsigned long now = millis();

  // 1‑minute inactivity?
  if (now - lastActivityTime >= INACTIVITY_TIMEOUT_MS)
  {
    Serial.println("1 min idle → sleeping");
    armDeepSleep_FireOnly();
    // never returns
  }

  bool firePressed = (digitalRead(PIN_FIRE) == LOW);
  bool leftPressed = (digitalRead(PIN_LEFT) == LOW);

  static unsigned long fireHoldStart = 0;
  static bool pairingDone = false;

  if (firePressed)
  {
    if (fireHoldStart == 0)
    {
      fireHoldStart = now;
      pairingDone = false; // allow pairing on each fresh press
      Serial.println("FIRE down → starting timer");
    }

    unsigned long held = now - fireHoldStart;
    // Serial.print("Held: "); Serial.print(held);
    // Serial.print(" ms, LEFT is "); Serial.println(leftPressed ? "LOW" : "HIGH");

    // ——— case A: FIRE + LEFT → RESTART after 6s ———
    if (leftPressed && held >= RESTART_HOLD_TIME)
    {
      Serial.println(">>> FIRE+LEFT 6s → restarting");
      M5.Beep.tone(1000, 200);
      delay(100);
      M5.Beep.mute();
      ESP.restart();
      // no return
    }
    // ——— case B: FIRE alone → PAIRING after 8 s ———
    else if (!leftPressed && !pairingDone && held >= PAIRING_MODE_HOLD_TIME)
    {
      pairingDone = true;
      Serial.println(">>> FIRE alone 5s → pairing mode");
      gamepad.deleteAllBonds();
      delay(100);
      gamepad.enterPairingMode();
      bleBeep();
    }
  }
  else
  {
    // FIRE released → reset
    if (fireHoldStart != 0)
    {
      Serial.println("FIRE released → resetting timers");
    }
    fireHoldStart = 0;
  }

  // — rest of your connected/reporting logic unchanged —
  bool connected = gamepad.isConnected();
  if (connected && !wasConnected)
  {
    Serial.println("Connected!");
    bleBeep();
  }
  wasConnected = connected;

  if (connected)
  {
    readInputsAndSendReport();
    if (now - lastBatteryUpdateTime > BATTERY_UPDATE_INTERVAL)
    {
      currentBatteryLevel = getBatteryVoltage();
      gamepad.setBatteryLevel(currentBatteryLevel);
      lastBatteryUpdateTime = now;
      Serial.print("Battery Level Updated: ");
      Serial.println(currentBatteryLevel);
    }
  }

  vTaskDelay(pdMS_TO_TICKS(30)); // 10ms sleep
}