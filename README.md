# FlowSense – Integrated Farm Water Management and Soil Adaptive System

FlowSense is a proposed ESP32-based **closed-loop irrigation** prototype for small and marginal farmers. It is designed to monitor soil and tank conditions, control irrigation, verify the response after watering, and flag **probable** irrigation faults.

> **Prototype status:** This repository is a starter implementation and documentation scaffold. Hardware pin assignments, sensor calibration, valve operation, and fault thresholds must be checked on the actual prototype. Do not claim field validation or confirmed fault diagnosis until tests have been completed.

## Main idea

**Sense → Analyse → Irrigate → Wait for soak lag → Verify → Flag probable fault → Repeat**

Unlike a simple timer controller, FlowSense checks sensor changes after irrigation. Its first version uses a rule-based algorithm; it does not require a trained AI/ML model or cloud service.

## Planned hardware

- ESP32 development board
- Capacitive soil-moisture sensor (one sensor for the initial prototype; a second depth can be added)
- DHT22 temperature/humidity sensor
- HC-SR04 ultrasonic sensor for tank-level change
- Relay module and compatible solenoid valve (only when safely integrated)
- LCD display (I2C version assumed in the starter code)
- Suitable power supply, wiring, tubing, and water container

## Repository structure

```text
FlowSense_GitHub_Starter/
├── README.md
├── LICENSE
├── .gitignore
├── firmware/
│   └── flowsense_esp32.ino
├── docs/
│   ├── architecture.md
│   └── test_plan.md
├── data/
│   └── sample_sensor_log.csv
└── tests/
    └── fault_logic_test_cases.md
```

## Important safety notes

- Never power a 12 V valve directly from an ESP32 GPIO.
- Use a correctly rated relay/driver and a suitable external supply; connect grounds as required by the chosen circuit design.
- Check relay active-high/active-low behaviour before connecting the valve.
- Do not operate water near exposed mains wiring.
- The HC-SR04 echo output is commonly 5 V; use a suitable voltage divider/level shifter before connecting it to an ESP32 input.
- Calibrate the soil sensor in the actual soil. Raw values differ between sensors and soil conditions.

## Firmware setup

1. Install the Arduino IDE and ESP32 board support on your computer.
2. Install compatible libraries for **DHT sensor**, **LiquidCrystal_I2C**, and their dependencies.
3. Open `firmware/flowsense_esp32.ino`.
4. Review the pin definitions at the top and change them to match your actual wiring.
5. Check the LCD I2C address (often `0x27` or `0x3F`) and display dimensions.
6. Upload with the valve disconnected initially. Open Serial Monitor at **115200 baud**.
7. Calibrate `SOIL_DRY_RAW` and `SOIL_WET_RAW`; the values in the file are examples only.
8. Test sensor readings and relay logic with a safe indicator/load before connecting the valve.
9. Run the cases in `docs/test_plan.md` and record actual results.

## How the first rule-based verification works

- At the start of an irrigation cycle, the firmware stores soil moisture and tank distance.
- After the irrigation run, it waits for a soak delay.
- It compares the soil moisture response and tank-level change.
- If the tank level does not change enough, it flags **possible no/low water delivery**.
- If water appears to have left the tank but soil moisture barely changes, it flags **possible delivery/infiltration issue**.
- If the soil sensor reading is outside the configured plausible range, it flags **possible sensor error**.

These are **probable fault flags**, not definitive diagnoses. For example, tank-level and one soil sensor alone cannot reliably distinguish a leak from a blocked dripper or poor infiltration. More sensors and controlled tests are needed to improve classification.

## Sample data

`data/sample_sensor_log.csv` contains illustrative sample rows only, not measured experimental results. Replace them with real readings collected from the prototype and document the test conditions.

## Validation and reporting

For each test, record:
- Date and test ID
- Soil type and initial moisture
- Tank level before and after irrigation
- Irrigation duration
- Soil reading before and after soak lag
- Expected outcome and observed outcome
- Fault flag and reviewer notes

Do not publish fabricated accuracy, water savings, or crop-yield improvements.

## Future enhancements

- Second soil sensor at root-zone depth
- GSM/SMS alerts
- Solar power and battery backup
- Better fault classification from labelled test data
- Optional AI/ML after sufficient real sensor data is collected
- Multi-zone irrigation

## License

This starter repository uses the MIT License. You may change the license if your team or institution requires a different arrangement.
