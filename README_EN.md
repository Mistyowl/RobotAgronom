![GitHub Release](https://img.shields.io/github/v/release/Mistyowl/RobotAgronom)
![GitHub License](https://img.shields.io/github/license/Mistyowl/RobotAgronom)
[![Translate README](https://img.shields.io/badge/README-ENGLISH-blueviolet.svg)](README_EN.md) 

This is an automatic translation, may be correct in som place.

# RobotAgronom
RobotAgronom is a reliable assistant in agronomy! 🌱
- Designed in Fusion 360, printed on Raise 3D Pro2
- It is controlled via a phone using a Web server

## What our robot can do:
#### 1. Automatic movement
- **Speed calibration:** The robot learns to accurately calculate the distance by moving forward for 10 seconds. After that, you specify how many meters it has traveled, and it records its speed in non-volatile memory. This way, the settings do not disappear after the device is turned off and rebooted.
- **Missions:** Set the total distance (for example, 100 meters) and the number of stops (for example, 5). The robot will evenly distribute the soil measuring points with the sensor and will move between them autonomously.
#### 2. Working with the soil sensor
At each stop, the robot:
1. Lowers the sensor into the ground.
2. Measures 9 parameters:
- Humidity, temperature, pH
- Nitrogen (N), phosphorus (P), Potassium (K)
- Electrical conductivity, Salinity, Total dissolved substances (TDS).
3. Raises the sensor and continues moving.
#### 3. Saving and viewing data
- All data is written to the SD card in JSON format. (you can view the file structure below)
- Real-time readings are displayed in the web interface. (will appear in the update)
- Files can be extracted and analyzed on a computer.
#### 4. Safety
- **Emergency stop:** Press the "Stop" button in the interface at any time.
- **Error protection:** The robot will not start a new mission until it completes the current one.

## JSON file structure (data.json)
``
[
  {
    "timestamp": 51262,
    "humi": 22.7,
    "temp": 21.9,
    "cond": 182,
    "phph": 5.3,
    "nitro": 0,
    "phos": 45,
    "pota": 37,
    "soli": 100,
    "tds": 91
  },
  {
    "timestamp": 69584,
    "humi": 22.7,
    "temp": 21.9,
    "cond": 182,
    "phph": 5.3,
    "nitro": 0,
    "phos": 45,
    "pota": 37,
    "soli": 100,
    "tds": 91
  }
]
``

## Component base
| № | Part name | Quantity | Total cost |
| - | ------------------- | ------ | ------------------: |
| 1. | ESP32 38PIN controller | 1 pc. | 545 ₽ |
| 2. | 28BYJ-48 stepper motor with ULN2003 driver | 1 pc. | 291 ₽ |
| 3. | JGY370 55 rpm gear motor, 6mm shaft | 2 pcs. | 1800 ₽ |
| 4. | L298N motor driver | 1 pc. | 320 ₽ |
| 5. | RS485 UART (TTL) converter | 1 pc. | 259 ₽ |
| 6. | RS485 Modbus Soil Sensor 7 in 1 | 1 pc. | 2958 ₽ |
| 7. | MINI SD Arduino Card Reader Module | 1 pc. | 205 ₽ |
| 8. | ABEC-7 608 RS Bearings | 1 pc. | 283 ₽ |
| 9. | GT2 10mm Open Toothed Belt, Reinforced with Polymer Fiber | 4 pcs. | 1260 ₽ |
| 10. | Rubber O-ring (outer diameter 12mm, thickness 2mm) | 100 pcs. | 400 ₽ |
| 11. | 18650 Battery Case | 2 pcs. | 306 ₽ |
| 12. | PETG Plastic Used for Printing | 3 kg. | 5700 ₽ |
| 13. | SD Card 16 GB | 1 pc. | 350 ₽ |
| 14. | Li-ion 18650 Battery | 3 pcs. | 720 ₽ |
| 15. | Voltage Converter LM2596 | 1 pc. | 142 ₽ |
|     |                          | Total: | 15 539 ₽ |

| Bolts | Quantity |
| ----- | -----: |
| M4x20 | 7 pcs. |
| M8x60 | 2 pcs. |
| M8x70 | 2 pcs. |
| M3x30 | 20 pcs. |
| Nuts | Qty |
| M3 | 16 pcs. |
| M4 | 5 pcs. |
| M8 | 6 pcs. |


## Instructions
#### Step 1. Connect
1. Turn on the robot.
2. Connect to Wi-Fi on your phone/laptop:
- **Network:** ``RobotControl``
- **Password:** ```12345678```
3. Open a browser and navigate to: ``http://192.168.4.1``
#### Step 2. Speed calibration
1. Press the **"Calibration check-in"** button.
2. The robot will drive for 10 seconds. Use a tape measure to measure how many meters he has traveled.
3. Enter this value in meters in the **"Real distance"** field and click **"Set calibration"**.
4. Now the robot knows its exact speed.
#### Step 3. Launch the mission
1. In the **"Mission Program" section** specify:
    - **Total distance** (for example, 100 meters).
    - **Number of measurements** (for example, 5 stops).
2. Press **"Start Mission"**.
3. The robot will start moving. At each stop point, he will:
- Stop.
    - Lowers the sensor.
    - Take measurements of the readings and record them on the sd card.
    - It will raise the sensor and move on.
#### Step 4. View the data
1. In the web interface you will see:
- Current sensor readings.
    - The time of the last update.
    - The status of the SD card.
2. To get historical data:
    - Remove the SD card from the robot.
    - Insert it into the computer file ``data.json`` contains all measurements.

## Web interface
![Web interface img](https://github.com/Mistyowl/RobotAgronom/blob/main/src/interface.jpg)

## Connection diagram
![Connection diagram img](https://github.com/Mistyowl/RobotAgronom/blob/main/src/wiring.png)

## Tips and Troubleshooting
- **The SD card is not detected:**
- Check if the card is connected.
    - Make sure that the card is fully inserted.
    - Make sure it is formatted in FAT32.
- **The robot is not moving:**
- Check the battery charge level.
    - Make sure that the motors are connected correctly.
- **There is no data in the interface:**
- Refresh the page (F5).
- Make sure that the sensor is fully lowered.

## Updates
* v1.0 - release
* v1.1 - fixed connection pins, added pinout diagram + minor changes
* v1.2 - updated README.md, added component base