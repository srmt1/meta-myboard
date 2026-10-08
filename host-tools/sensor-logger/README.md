# Ubuntu Sensor Logger

Python MQTT subscriber that receives sensor readings from the Raspberry Pi
and stores them in an SQLite database.

## Architecture

Raspberry Pi sensor-monitor
    -> MQTT
    -> Mosquitto broker on Ubuntu
    -> sensor_logger.py
    -> SQLite database

## Requirements

- Ubuntu Linux
- Python 3
- Paho MQTT Python client
- SQLite support (included with Python)

## Configuration

Edit sensor_logger.py to configure:

- MQTT broker address and port
- MQTT username and password
- MQTT topic
- SQLite database path

Do not commit real MQTT passwords or other credentials.

## Running

Install the Ubuntu dependencies:

    sudo apt install python3-paho-mqtt

Run the logger:

    python3 sensor_logger.py

Stop it with Ctrl+C.

## Database

The database is stored at the path specified by DATABASE in sensor_logger.py.

The readings table contains:

- timestamp
- AHT20 temperature
- AHT20 relative humidity
- BMP280 temperature
- BMP280 atmospheric pressure

## Current status

- MQTT connection and subscription working
- JSON sensor messages parsed
- Sensor readings stored in SQLite
- Clean shutdown with Ctrl+C

The logger currently runs interactively; a systemd service is not configured.
