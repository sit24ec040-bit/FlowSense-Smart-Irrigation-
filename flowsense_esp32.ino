/*
  PROJECT: FlowSense
  Integrated Farm Water Management and Soil Adaptive System

  CONTROLLER: ESP32

  CORE FEATURES:
  1. Soil moisture monitoring
  2. DHT22 temperature and humidity monitoring
  3. Local VPD calculation
  4. Automatic relay/valve control
  5. Tank-level-based water-delivery estimation
  6. Closed-loop post-irrigation verification
  7. Probable irrigation fault classification
  8. LCD and Serial Monitor status display

  IMPORTANT:
  - Calibrate all sensor values before actual use.
  - Fault classifications are estimates, not proof of a fault.
  - Test the relay without the valve connected first.
  - No cloud, Wi-Fi, or internet connection is required.
*/

#include <Arduino.h>
#include <Wire.h>
#include <DHT.h>
#include <LiquidCrystal_I2C.h>

// =====================================================
// 1. PIN CONFIGURATION
// =====================================================

const int SOIL_PIN = 34;       // Capacitive soil sensor, ADC input
const int DHT_PIN = 4;         // DHT22 data pin
const int TRIG_PIN = 5;        // HC-SR04 trigger
const int ECHO_PIN = 18;       // HC-SR04 echo through voltage divider
const int RELAY_PIN = 26;      // Relay input

const int LCD_SDA_PIN = 21;    // I2C LCD SDA
const int LCD_SCL_PIN = 22;    // I2C LCD SCL

// =====================================================
// 2. HARDWARE SETTINGS
// =====================================================

#define DHT_TYPE DHT22

// Most relay modules are active LOW.
// Change these two values if your relay behaves differently.
const int RELAY_ON_LEVEL = LOW;
const int RELAY_OFF_LEVEL = HIGH;

// Common I2C LCD addresses are 0x27 and 0x3F.
// Confirm the address of your actual LCD.
const uint8_t LCD_ADDRESS = 0x27;

const int LCD_COLUMNS = 16;
const int LCD_ROWS = 2;

DHT dht(DHT_PIN, DHT_TYPE);
LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLUMNS, LCD_ROWS);

// =====================================================
// 3. SOIL SENSOR CALIBRATION
// =====================================================

// EXAMPLE VALUES ONLY.
// Replace these with readings from YOUR sensor.
//
// DRY: sensor reading in dry soil.
// WET: sensor reading in wet soil.
//
// Many capacitive sensors give a higher ADC value when dry
// and a lower value when wet. Confirm this experimentally.

const int SOIL_DRY_RAW = 3000;
const int SOIL_WET_RAW = 1400;

// Valid ESP32 ADC range is approximately 0 to 4095.
// Use these bounds to detect obviously abnormal readings.
const int SOIL_ADC_MIN_VALID = 100;
const int SOIL_ADC_MAX_VALID = 4000;

// =====================================================
// 4. TANK CONFIGURATION
// =====================================================

// Distance from the ultrasonic sensor to the tank bottom
// when the tank is empty.
//
// Measure these dimensions on your actual tank.

const float TANK_DEPTH_CM = 20.0;

// Sensor-to-water distance when the tank is considered full.
// Set to 2 cm only if your actual full-water distance is 2 cm.
const float TANK_FULL_DISTANCE_CM = 2.0;

// A small tank-level percentage change can be measurement noise.
const float MIN_TANK_DROP_PERCENT = 3.0;

// =====================================================
// 5. IRRIGATION SETTINGS
// =====================================================

// Base soil moisture threshold.
// Irrigation is considered when moisture is at or below this.
const float BASE_DRY_THRESHOLD_PERCENT = 35.0;

// VPD adjustment: higher VPD raises the dry threshold,
// allowing irrigation to be considered sooner.
// This is a prototype rule, not a crop-specific ET model.
const float VPD_THRESHOLD_FACTOR = 3.0;
const float MAX_VPD_THRESHOLD_INCREASE = 10.0;

// Safety limit for a single irrigation cycle.
const unsigned long MAX_WATERING_TIME_MS = 10000UL;

// Allow the soil to absorb water before verification.
const unsigned long SOAK_TIME_MS = 30000UL;

// Minimum measured moisture increase considered a response.
const float MIN_MOISTURE_RISE_PERCENT = 5.0;

// Wait between automatic decisions.
const unsigned long DECISION_INTERVAL_MS = 5000UL;

// Sensor sampling intervals.
const unsigned long DHT_INTERVAL_MS = 2500UL;
const unsigned long ULTRASONIC_INTERVAL_MS = 1000UL;

// =====================================================
// 6. SENSOR OBJECTS AND GLOBAL VARIABLES
// =====================================================

enum SystemState {
  MONITORING,
  IRRIGATING,
  SOAKING,
  VERIFYING,
  FAULT_LOCKOUT
};

SystemState systemState = MONITORING;

int soilRaw = 0;
float soilMoisturePercent = -1.0;

float temperatureC = NAN;
float humidityPercent = NAN;
float vpdKPa = NAN;

float tankDistanceCm = NAN;
float tankLevelPercent = -1.0;

float moistureBeforeIrrigation = -1.0;
float moistureAfterIrrigation = -1.0;

float tankBeforeIrrigation = -1.0;
float tankAfterIrrigation = -1.0;

float estimatedTankDropPercent = 0.0;
float moistureRisePercent = 0.0;

String lastStatus = "SYSTEM STARTING";
String lastFault = "NONE";

unsigned long wateringStartMs = 0;
unsigned long soakStartMs = 0;
unsigned long lastDecisionMs = 0;
unsigned long lastDHTReadMs = 0;
unsigned long lastUltrasonicReadMs = 0;
unsigned long lastLCDUpdateMs = 0;

bool dhtReadingValid = false;
bool soilReadingValid = false;
bool tankReadingValid = false;

bool wateringCycleStarted = false;

// =====================================================
// 7. RELAY CONTROL
// =====================================================

void valveOff() {
  digitalWrite(RELAY_PIN, RELAY_OFF_LEVEL);
}

void valveOn() {
  digitalWrite(RELAY_PIN, RELAY_ON_LEVEL);
}

// =====================================================
// 8. SOIL MOISTURE READING
// =====================================================

void readSoilMoisture() {
  soilRaw = analogRead(SOIL_PIN);

  if (soilRaw < SOIL_ADC_MIN_VALID ||
      soilRaw > SOIL_ADC_MAX_VALID) {
    soilReadingValid = false;
    soilMoisturePercent = -1.0;
    return;
  }

  if (SOIL_DRY_RAW == SOIL_WET_RAW) {
    soilReadingValid = false;
    soilMoisturePercent = -1.0;
    return;
  }

  // Convert raw ADC reading to an approximate percentage.
  // This assumes dry readings are higher than wet readings.
  float moisture =
      100.0 * (SOIL_DRY_RAW - soilRaw) /
      (float)(SOIL_DRY_RAW - SOIL_WET_RAW);

  soilMoisturePercent = constrain(moisture, 0.0, 100.0);
  soilReadingValid = true;
}

// =====================================================
// 9. DHT22 READING AND VPD CALCULATION
// =====================================================

void readEnvironment() {
  unsigned long now = millis();

  if (now - lastDHTReadMs < DHT_INTERVAL_MS) {
    return;
  }

  lastDHTReadMs = now;

  float newTemperature = dht.readTemperature();
  float newHumidity = dht.readHumidity();

  if (isnan(newTemperature) || isnan(newHumidity) ||
      newHumidity < 0.0 || newHumidity > 100.0 ||
      newTemperature < -20.0 || newTemperature > 80.0) {
    dhtReadingValid = false;
    vpdKPa = NAN;
    return;
  }

  temperatureC = newTemperature;
  humidityPercent = newHumidity;

  // Saturation vapour pressure, in kPa.
  float saturationVP =
      0.6108 * exp((17.27 * temperatureC) /
                   (temperatureC + 237.3));

  // Actual vapour pressure, in kPa.
  float actualVP =
      saturationVP * (humidityPercent / 100.0);

  // Vapour Pressure Deficit, in kPa.
  vpdKPa = saturationVP - actualVP;

  if (vpdKPa < 0.0) {
    vpdKPa = 0.0;
  }

  dhtReadingValid = true;
}

// =====================================================
// 10. ULTRASONIC TANK-LEVEL READING
// =====================================================

float measureTankDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(3);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // Timeout avoids waiting indefinitely.
  unsigned long duration = pulseIn(ECHO_PIN, HIGH, 30000UL);

  if (duration == 0) {
    return NAN;
  }

  // Sound travels out and back.
  float distanceCm = duration * 0.0343 / 2.0;

  if (distanceCm < 1.0 || distanceCm > 400.0) {
    return NAN;
  }

  return distanceCm;
}

void readTankLevel() {
  unsigned long now = millis();

  if (now - lastUltrasonicReadMs < ULTRASONIC_INTERVAL_MS) {
    return;
  }

  lastUltrasonicReadMs = now;

  float distance = measureTankDistance();

  if (isnan(distance)) {
    tankReadingValid = false;
    tankLevelPercent = -1.0;
    return;
  }

  // Water height = tank depth - sensor-to-water distance.
  float waterHeight = TANK_DEPTH_CM - distance;

  // Calculate an approximate level percentage.
  float level =
      100.0 * waterHeight / TANK_DEPTH_CM;

  if (distance < TANK_FULL_DISTANCE_CM - 1.0 ||
      distance > TANK_DEPTH_CM + 3.0) {
    tankReadingValid = false;
    tankLevelPercent = -1.0;
    return;
  }

  tankLevelPercent = constrain(level, 0.0, 100.0);
  tankDistanceCm = distance;
  tankReadingValid = true;
}

// =====================================================
// 11. LOCAL WATER-DEMAND THRESHOLD
// =====================================================

float calculateDryThreshold() {
  float threshold = BASE_DRY_THRESHOLD_PERCENT;

  if (dhtReadingValid && !isnan(vpdKPa)) {
    float increase = vpdKPa * VPD_THRESHOLD_FACTOR;

    if (increase > MAX_VPD_THRESHOLD_INCREASE) {
      increase = MAX_VPD_THRESHOLD_INCREASE;
    }

    threshold += increase;
  }

  return constrain(threshold, 0.0, 100.0);
}

// =====================================================
// 12. SERIAL MONITOR LOGGING
// =====================================================

void printSensorData() {
  Serial.println();
  Serial.println("========== FLOWSENSE SENSOR DATA ==========");

  Serial.print("Soil ADC: ");
  Serial.println(soilRaw);

  Serial.print("Soil moisture (%): ");
  if (soilReadingValid) {
    Serial.println(soilMoisturePercent, 1);
  } else {
    Serial.println("INVALID");
  }

  Serial.print("Temperature (C): ");
  if (dhtReadingValid) {
    Serial.println(temperatureC, 1);
  } else {
    Serial.println("INVALID");
  }

  Serial.print("Relative humidity (%): ");
  if (dhtReadingValid) {
    Serial.println(humidityPercent, 1);
  } else {
    Serial.println("INVALID");
  }

  Serial.print("VPD (kPa): ");
  if (dhtReadingValid) {
    Serial.println(vpdKPa, 3);
  } else {
    Serial.println("INVALID");
  }

  Serial.print("Tank distance (cm): ");
  if (tankReadingValid) {
    Serial.println(tankDistanceCm, 1);
  } else {
    Serial.println("INVALID");
  }

  Serial.print("Tank level (%): ");
  if (tankReadingValid) {
    Serial.println(tankLevelPercent, 1);
  } else {
    Serial.println("INVALID");
  }

  Serial.print("System state: ");
  Serial.println((int)systemState);

  Serial.print("Status: ");
  Serial.println(lastStatus);

  Serial.print("Last probable fault: ");
  Serial.println(lastFault);

  Serial.println("===========================================");
}

// =====================================================
// 13. START IRRIGATION
// =====================================================

void startIrrigation() {
  if (!soilReadingValid) {
    lastStatus = "SOIL SENSOR ERROR";
    lastFault = "SOIL SENSOR ERROR";
    valveOff();
    systemState = FAULT_LOCKOUT;
    return;
  }

  if (!tankReadingValid) {
    lastStatus = "TANK SENSOR ERROR";
    lastFault = "TANK SENSOR ERROR";
    valveOff();
    systemState = FAULT_LOCKOUT;
    return;
  }

  // Record baseline readings before watering.
  moistureBeforeIrrigation = soilMoisturePercent;
  tankBeforeIrrigation = tankLevelPercent;

  wateringStartMs = millis();
  wateringCycleStarted = true;

  valveOn();

  lastStatus = "IRRIGATION STARTED";
  lastFault = "NONE";

  systemState = IRRIGATING;

  Serial.println();
  Serial.println(">>> IRRIGATION STARTED");
  Serial.print("Baseline soil moisture: ");
  Serial.println(moistureBeforeIrrigation, 1);

  Serial.print("Baseline tank level: ");
  Serial.println(tankBeforeIrrigation, 1);
}

// =====================================================
// 14. STOP IRRIGATION AND START SOAK PERIOD
// =====================================================

void stopIrrigation() {
  valveOff();

  soakStartMs = millis();
  systemState = SOAKING;

  lastStatus = "SOAKING - WAIT";

  Serial.println(">>> VALVE OFF");
  Serial.println(">>> SOAK PERIOD STARTED");
}

// =====================================================
// 15. FAULT CLASSIFICATION
// =====================================================

void classifyIrrigationResult() {
  valveOff();

  // A failed reading must not be treated as a successful cycle.
  if (!soilReadingValid) {
    lastFault = "SOIL SENSOR ERROR";
    lastStatus = "VERIFY FAILED: SOIL";
    systemState = FAULT_LOCKOUT;
    return;
  }

  if (!tankReadingValid) {
    lastFault = "TANK SENSOR ERROR";
    lastStatus = "VERIFY FAILED: TANK";
    systemState = FAULT_LOCKOUT;
    return;
  }

  moistureAfterIrrigation = soilMoisturePercent;
  tankAfterIrrigation = tankLevelPercent;

  // A positive drop means the measured tank level decreased.
  estimatedTankDropPercent =
      tankBeforeIrrigation - tankAfterIrrigation;

  // A positive rise means measured soil moisture increased.
  moistureRisePercent =
      moistureAfterIrrigation - moistureBeforeIrrigation;

  Serial.println();
  Serial.println("========== POST-IRRIGATION VERIFICATION ==========");

  Serial.print("Soil before (%): ");
  Serial.println(moistureBeforeIrrigation, 1);

  Serial.print("Soil after (%): ");
  Serial.println(moistureAfterIrrigation, 1);

  Serial.print("Moisture rise (% points): ");
  Serial.println(moistureRisePercent, 1);

  Serial.print("Tank level before (%): ");
  Serial.println(tankBeforeIrrigation, 1);

  Serial.print("Tank level after (%): ");
  Serial.println(tankAfterIrrigation, 1);

  Serial.print("Tank level drop (% points): ");
  Serial.println(estimatedTankDropPercent, 1);

  /*
    RULE-BASED FAULT LOGIC

    CASE A:
    Tank level barely changes and soil moisture barely rises.
    Possible causes: blocked dripper, closed valve, empty supply,
    or no actual water delivery.

    CASE B:
    Tank level decreases but soil moisture barely rises.
    Possible causes: leakage, water routed elsewhere, poor
    infiltration, insufficient soak time, or sensor placement.
    One soil sensor cannot reliably distinguish these causes.

    CASE C:
    Soil moisture rises by the minimum threshold.
    The measured response is consistent with successful watering.

    These are PROBABLE classifications, not definitive diagnoses.
  */

  if (estimatedTankDropPercent < MIN_TANK_DROP_PERCENT &&
      moistureRisePercent < MIN_MOISTURE_RISE_PERCENT) {

    lastFault = "NO DELIVERY / BLOCKAGE?";
    lastStatus = "FAULT: CHECK WATER PATH";
    systemState = FAULT_LOCKOUT;

  } else if (estimatedTankDropPercent >= MIN_TANK_DROP_PERCENT &&
             moistureRisePercent < MIN_MOISTURE_RISE_PERCENT) {

    lastFault = "LEAK / INFILTRATION?";
    lastStatus = "FAULT: CHECK SOIL & PIPES";
    systemState = FAULT_LOCKOUT;

  } else if (moistureRisePercent >= MIN_MOISTURE_RISE_PERCENT) {

    lastFault = "NONE DETECTED";
    lastStatus = "IRRIGATION VERIFIED";
    systemState = MONITORING;

  } else {

    lastFault = "INCONCLUSIVE RESPONSE";
    lastStatus = "CHECK SENSOR READINGS";
    systemState = FAULT_LOCKOUT;
  }

  Serial.print("Verification status: ");
  Serial.println(lastStatus);

  Serial.print("Probable fault: ");
  Serial.println(lastFault);

  Serial.println("==================================================");
}

// =====================================================
// 16. LCD DISPLAY
// =====================================================

void updateLCD() {
  unsigned long now = millis();

  if (now - lastLCDUpdateMs < 1000UL) {
    return;
  }

  lastLCDUpdateMs = now;

  lcd.clear();

  if (systemState == IRRIGATING) {
    lcd.setCursor(0, 0);
    lcd.print("IRRIGATING...");

    lcd.setCursor(0, 1);
    lcd.print("Soil:");
    if (soilReadingValid) {
      lcd.print(soilMoisturePercent, 0);
      lcd.print("%");
    } else {
      lcd.print("ERR");
    }

  } else if (systemState == SOAKING) {
    lcd.setCursor(0, 0);
    lcd.print("SOAKING...");

    lcd.setCursor(0, 1);
    lcd.print("Verification wait");

  } else if (systemState == VERIFYING) {
    lcd.setCursor(0, 0);
    lcd.print("VERIFYING...");

    lcd.setCursor(0, 1);
    lcd.print("Checking response");

  } else if (systemState == FAULT_LOCKOUT) {
    lcd.setCursor(0, 0);
    lcd.print("FAULT DETECTED");

    lcd.setCursor(0, 1);
    lcd.print(lastFault.substring(0, 16));

  } else {
    lcd.setCursor(0, 0);
    lcd.print("Soil:");

    if (soilReadingValid) {
      lcd.print(soilMoisturePercent, 0);
      lcd.print("%");
    } else {
      lcd.print("ERR");
    }

    lcd.setCursor(0, 1);
    if (tankReadingValid) {
      lcd.print("Tank:");
      lcd.print(tankLevelPercent, 0);
      lcd.print("% ");
    } else {
      lcd.print("Tank:ERR ");
    }

    if (dhtReadingValid) {
      lcd.print("V");
      lcd.print(vpdKPa, 1);
    }
  }
}

// =====================================================
// 17. SETUP
// =====================================================

void setup() {
  Serial.begin(115200);

  pinMode(SOIL_PIN, INPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);

  // Always switch the valve OFF during startup.
  valveOff();

  digitalWrite(TRIG_PIN, LOW);

  analogReadResolution(12);

  // Initialize I2C on the selected ESP32 pins.
  Wire.begin(LCD_SDA_PIN, LCD_SCL_PIN);

  lcd.init();
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print("FlowSense");
  lcd.setCursor(0, 1);
  lcd.print("Starting...");

  dht.begin();

  delay(1500);

  Serial.println();
  Serial.println("=========================================");
  Serial.println(" FLOWSENSE ESP32 CONTROLLER");
  Serial.println(" Closed-loop irrigation + fault checking");
  Serial.println(" Local operation - no cloud required");
  Serial.println("=========================================");

  readSoilMoisture();
  readEnvironment();
  readTankLevel();

  lastStatus = "SYSTEM READY";
  lastDecisionMs = millis();
}

// =====================================================
// 18. MAIN CLOSED-LOOP CONTROL
// =====================================================

void loop() {
  unsigned long now = millis();

  // Keep the relay OFF whenever the controller is locked out.
  if (systemState == FAULT_LOCKOUT) {
    valveOff();

    readSoilMoisture();
    readEnvironment();
    readTankLevel();

    updateLCD();

    // Print fault state periodically; no automatic restart.
    if (now - lastDecisionMs >= DECISION_INTERVAL_MS) {
      lastDecisionMs = now;
      printSensorData();
    }

    return;
  }

  // Continuously update sensor readings.
  readSoilMoisture();
  readEnvironment();
  readTankLevel();

  // Update display.
  updateLCD();

  // Log data periodically.
  if (now - lastDecisionMs >= DECISION_INTERVAL_MS) {
    lastDecisionMs = now;
    printSensorData();
  }

  // ---------------------------------------------------
  // STATE 1: MONITORING
  // ---------------------------------------------------

  if (systemState == MONITORING) {

    // Do not irrigate using invalid soil/tank readings.
    if (!soilReadingValid) {
      lastFault = "SOIL SENSOR ERROR";
      lastStatus = "SOIL READING INVALID";
      valveOff();
      systemState = FAULT_LOCKOUT;
      return;
    }

    if (!tankReadingValid) {
      lastFault = "TANK SENSOR ERROR";
      lastStatus = "TANK READING INVALID";
      valveOff();
      systemState = FAULT_LOCKOUT;
      return;
    }

    float dryThreshold = calculateDryThreshold();

    Serial.print("Current dry threshold (%): ");
    Serial.println(dryThreshold, 1);

    if (soilMoisturePercent <= dryThreshold) {
      startIrrigation();
    } else {
      lastStatus = "SOIL MOISTURE OK";
      valveOff();
    }
  }

  // ---------------------------------------------------
  // STATE 2: IRRIGATING
  // ---------------------------------------------------

  else if (systemState == IRRIGATING) {

    // Stop if the critical readings become invalid.
    if (!soilReadingValid || !tankReadingValid) {
      valveOff();

      lastFault = "SENSOR ERROR DURING WATERING";
      lastStatus = "WATERING STOPPED";
      systemState = FAULT_LOCKOUT;
      return;
    }

    // Stop at the configured maximum watering duration.
    if (now - wateringStartMs >= MAX_WATERING_TIME_MS) {
      stopIrrigation();
    }
  }

  // ---------------------------------------------------
  // STATE 3: SOAKING
  // ---------------------------------------------------

  else if (systemState == SOAKING) {

    valveOff();

    if (now - soakStartMs >= SOAK_TIME_MS) {
      systemState = VERIFYING;
      lastStatus = "VERIFYING RESPONSE";

      Serial.println(">>> SOAK PERIOD COMPLETE");
      Serial.println(">>> POST-IRRIGATION CHECK STARTED");
    }
  }

  // ---------------------------------------------------
  // STATE 4: VERIFICATION
  // ---------------------------------------------------

  else if (systemState == VERIFYING) {

    valveOff();

    // Take fresh readings after the soak period.
    readSoilMoisture();
    readEnvironment();
    readTankLevel();

    classifyIrrigationResult();

    // A successful verification returns to monitoring.
    // Probable faults remain locked out until manually reset
    // or the controller is restarted after checking the setup.
  }
}
