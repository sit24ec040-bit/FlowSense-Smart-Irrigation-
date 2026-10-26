# Fault Logic Test Cases

These are conceptual cases for evaluating the rule logic, not evidence that the hardware has passed testing.

| Case | Tank level change | Soil moisture response | Expected rule-based flag | Limitation |
|---|---|---|---|---|
| Normal response | Measurable water withdrawn | Moisture rises | Possible irrigation success | Confirm with repeated tests |
| Little/no water withdrawn | Very small change | Little/no rise | Possible low delivery | Could also be a valve/power/control issue |
| Water withdrawn | Moisture barely changes | Low rise | Check flow/soil response | Cannot uniquely distinguish leak, blockage, poor infiltration, or sensor placement |
| Invalid ultrasonic reading | Missing/invalid distance | Any | Check tank sensor | Inspect mounting, wiring, and echo level |
| Invalid DHT22 reading | N/A | Any | Check DHT22 | DHT22 is not used to confirm water delivery in this starter |
| Moisture value implausible | Any | Out-of-range or unstable raw data | Check soil sensor | Calibrate and repeat before classifying |

## Reporting rule
Use wording such as “probable blockage” or “possible delivery issue.” Do not state that a particular fault has been conclusively identified unless the test setup provides enough independent evidence.
