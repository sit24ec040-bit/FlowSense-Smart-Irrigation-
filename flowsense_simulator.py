def flowsense_simulator(soil_moisture, tank_level, vpd_high,
                        tank_drop=0, moisture_rise=0,
                        sensor_valid=True):
    print("\n--- FlowSense Simulation ---")

    if not sensor_valid:
        print("Result: SENSOR ERROR")
        print("Irrigation: OFF")
        return

    if tank_level < 10:
        print("Result: LOW WATER WARNING")
        print("Irrigation: OFF")
        return

    if soil_moisture > 35:
        print("Result: SOIL MOISTURE SUFFICIENT")
        print("Irrigation: OFF")
        return

    print("Soil is dry. Irrigation requested.")
    print("Simulating irrigation and post-soak verification...")

    if tank_drop < 3 and moisture_rise < 5:
        print("Result: POSSIBLE BLOCKAGE / NO DELIVERY")
    elif tank_drop >= 3 and moisture_rise < 5:
        print("Result: POSSIBLE LEAK / POOR INFILTRATION")
    else:
        print("Result: IRRIGATION VERIFIED")

    print("Simulation complete.")


# Example test: dry soil, enough tank water,
# tank level drops, and soil moisture rises.
flowsense_simulator(
    soil_moisture=20,
    tank_level=80,
    vpd_high=True,
    tank_drop=15,
    moisture_rise=12
)
