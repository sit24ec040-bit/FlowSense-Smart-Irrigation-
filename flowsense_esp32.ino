/*
  FlowSense – ESP32 closed-loop irrigation starter firmware

  IMPORTANT:
  - This is a configurable starter, not a verified drop-in build.
  - Check every pin against your actual wiring before powering hardware.
  - Test with the valve disconnected first.
  - Calibrate soil sensor raw values and tank thresholds.
  - LCD code assumes an I2C 16x2 display.
*/

#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ---------- CONFIGURE THESE PINS FOR YOUR ACTUAL WIRING ----------
const int SOIL_PIN = 34;       // ADC-capable ESP32 input
const int DHT_PIN = 4;         // Example GPIO; change if needed
const int TRIG_PIN = 5;        // Example GPIO; change if needed
const int ECHO_PIN = 18;       // Use level shifting if sensor echo is 5 V
const int RELAY_PIN = 23;      // Example GPIO; change if needed

// Many relay boards are active LOW. Confirm before use.
const int RELAY_ON = LOW;
const int RELAY_OFF = HIGH;

// LCD I2C address may be 0x27 or 0x3F depending on module.
LiquidCrystal_I2C lcd(0x27, 16, 2);
#define DHTTYPE DHT22
DHT dht(DHT_PIN, DHTTYPE);

// ---------- CALIBRATION / CONTROL SETTINGS (EXAMPLES ONLY) ----------
const int SOIL_DRY_RAW = 3000;  // Replace after dry-soil calibration
const int SOIL_WET_RAW = 1400;  // Replace after wet-soil calibration
const int SOIL_NEEDS_WATER_PERCENT = 35;
const int MIN_MOISTURE_RISE_PERCENT = 5;
const float MIN_TANK_LEVEL_CHANGE_CM = 1.0;
const unsigned long IRRIGATION_DURATION_MS = 5000; // Short safe test duration
const unsigned long SOAK_DELAY_MS = 30000;         // Short demo delay; tune experimentally
const unsigned long SENSOR_INTERVAL_MS = 2000;
const unsigned long MAX_ECHO_US = 30000;

// These are demo thresholds, not agronomic recommendations.
unsigned long lastReadMs = 0;
bool irrigationInProgress = false;

struct Snapshot {
  int soilPercent;
  float tankDistanceCm;
};

int readSoilPercent() {
  int raw = analogRead(SOIL_PIN);
  // Mapping assumes dry raw > wet raw. Recheck this for your sensor.
  int percent = map(raw, SOIL_DRY_RAW, SOIL_WET_RAW, 0, 100);
  return constrain(percent, 0, 100);
}

float readTankDistanceCm() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(3);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  unsigned long duration = pulseIn(ECHO_PIN, HIGH, MAX_ECHO_US);
  if (duration == 0) return -1.0; // Invalid/no echo
  return duration * 0.0343f / 2.0f;
}

Snapshot readSnapshot() {
  Snapshot s;
  s.soilPercent = readSoilPercent();
  s.tankDistanceCm = readTankDistanceCm();
  return s;
}

void setValve(bool openValve) {
  digitalWrite(RELAY_PIN, openValve ? RELAY_ON : RELAY_OFF);
}

void printSnapshot(const Snapshot &s, float temperatureC, float humidity) {
  Serial.print("soil_percent=");
  Serial.print(s.soilPercent);
  Serial.print(", tank_distance_cm=");
  Serial.print(s.tankDistanceCm);
  Serial.print(", temp_c=");
  Serial.print(temperatureC);
  Serial.print(", humidity_percent=");
  Serial.println(humidity);
}

void displayStatus(const String &line1, const String &line2) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1.substring(0, 16));
  lcd.setCursor(0, 1);
  lcd.print(line2.substring(0, 16));
}

String classifyProbableFault(const Snapshot &before, const Snapshot &after) {
  if (before.tankDistanceCm < 0 || after.tankDistanceCm < 0) {
    return "CHECK TANK SENSOR";
  }

  // A lower distance after irrigation means the water surface rose.
  // This simple demo assumes the sensor is mounted above the tank.
  float levelRiseCm = before.tankDistanceCm - after.tankDistanceCm;
  int moistureRise = after.soilPercent - before.soilPercent;

  if (levelRiseCm < MIN_TANK_LEVEL_CHANGE_CM && moistureRise < MIN_MOISTURE_RISE_PERCENT) {
    return "LOW DELIVERY?";
  }
  if (levelRiseCm >= MIN_TANK_LEVEL_CHANGE_CM && moistureRise < MIN_MOISTURE_RISE_PERCENT) {
    return "CHECK FLOW/SOIL";
  }
  if (moistureRise >= MIN_MOISTURE_RISE_PERCENT) {
    return "IRRIGATION OK?";
  }
  return "CHECK SENSORS";
}

void runIrrigationCycle(const Snapshot &before) {
  Serial.println("Irrigation requested. Opening relay for short demo interval.");
  displayStatus("Irrigating...", "Valve ON");
  setValve(true);
  delay(IRRIGATION_DURATION_MS);
  setValve(false);
  Serial.println("Valve OFF. Waiting for soak lag.");
  displayStatus("Soak delay...", "Please wait");
  delay(SOAK_DELAY_MS);

  Snapshot after = readSnapshot();
  float tempC = dht.readTemperature();
  float humidity = dht.readHumidity();
  printSnapshot(after, tempC, humidity);

  String result = classifyProbableFault(before, after);
  Serial.print("verification_result=");
  Serial.println(result);
  displayStatus("Verify result:", result);
  delay(3000);
}

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);
  setValve(false);

  dht.begin();
  Wire.begin();
  lcd.init();
  lcd.backlight();
  displayStatus("FlowSense", "Starting...");
  delay(1500);

  Serial.println("FlowSense starter firmware");
  Serial.println("Confirm wiring, calibration, and safe relay behaviour before use.");
}

void loop() {
  if (millis() - lastReadMs < SENSOR_INTERVAL_MS) return;
  lastReadMs = millis();

  Snapshot current = readSnapshot();
  float tempC = dht.readTemperature();
  float humidity = dht.readHumidity();

  printSnapshot(current, tempC, humidity);

  if (isnan(tempC) || isnan(humidity)) {
    displayStatus("DHT22 error", "Check wiring");
  } else {
    displayStatus("Soil:" + String(current.soilPercent) + "%",
                  "Tank:" + String(current.tankDistanceCm, 1) + "cm");
  }

  // Starter decision: moisture threshold only. VPD-based demand analysis
  // is documented as a planned refinement and is not implemented here.
  // Do not treat this simple threshold as a crop-specific irrigation prescription.
  if (current.soilPercent < SOIL_NEEDS_WATER_PERCENT &&
      current.tankDistanceCm > 0 &&
      !irrigationInProgress) {
    irrigationInProgress = true;
    runIrrigationCycle(current);
    irrigationInProgress = false;
  }
}
