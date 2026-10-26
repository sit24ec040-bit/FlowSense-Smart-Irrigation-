# FlowSense Test Plan

Complete these tests only after reviewing wiring and electrical safety. Keep the valve disconnected for initial sensor tests.

| Test ID | Test setup | Expected observation | Actual result |
|---|---|---|---|
| T01 | Power ESP32 with valve disconnected | Board starts; Serial Monitor prints startup message | Pending |
| T02 | Soil sensor in relatively dry soil | Reading changes and calibrated percentage is plausible | Pending |
| T03 | Soil sensor in wetter soil | Reading changes in the expected direction after calibration | Pending |
| T04 | DHT22 connected | Temperature and humidity readings appear; no NaN error | Pending |
| T05 | Ultrasonic sensor facing tank water | Stable distance readings; invalid echo is flagged | Pending |
| T06 | Relay tested with safe indicator/load | Relay ON/OFF matches configured active level | Pending |
| T07 | Controlled irrigation demonstration | Valve switches only for configured duration | Pending |
| T08 | After watering and soak delay | Before/after readings are logged | Pending |
| T09 | Simulated low delivery | Algorithm flags a probable low-delivery condition | Pending |
| T10 | Sensor disconnected/invalid reading | Error/status is visible; do not trust the result | Pending |

## Data to record
Test ID, date, soil type, sensor raw value, calibrated moisture %, temperature, relative humidity, tank distance before/after, irrigation duration, soak delay, expected result, observed result, and notes.

Do not mark tests as passed until they have actually been performed. Replace “Pending” with the real outcome and retain unexpected results.
