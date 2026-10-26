# FlowSense System Architecture

## Purpose
FlowSense aims to add post-irrigation verification and probable fault flagging to a local irrigation controller.

## Data flow
1. **Inputs:** soil-moisture sensor, DHT22 temperature/humidity sensor, ultrasonic tank-level sensor.
2. **Controller:** ESP32 reads sensors and applies configurable threshold/rule logic.
3. **Actuation:** ESP32 output drives a relay/driver, which switches a compatible solenoid valve.
4. **Verification:** after a soak delay, the system compares pre- and post-irrigation readings.
5. **Output:** LCD and Serial Monitor show readings and a probable status/fault flag.
6. **Repeat:** monitoring continues locally; cloud or internet service is not required.

## Important interpretation limits
- One soil sensor provides only a local measurement, not a reliable profile of the whole root zone.
- A tank-level change estimates water withdrawn from the tank; it does not directly measure delivered flow at the dripper.
- Little soil-moisture rise can have multiple causes, including sensor placement, soil type, soak delay, leakage, blockage, or poor infiltration.
- Therefore, the first version reports probable conditions rather than definitive fault diagnoses.
- VPD calculation and a crop-specific water-volume model are not implemented in the starter firmware. They are possible future additions after calibration and testing.

## Wiring safety
Use a suitable driver/relay and external supply for the valve. Never drive a valve directly from an ESP32 GPIO. Ensure ESP32 inputs are not exposed to 5 V signals.
