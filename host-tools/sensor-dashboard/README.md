# Sensor Dashboard

A local web dashboard for visualising environmental sensor data collected
from a Raspberry Pi running a custom Yocto Linux image.

## Features

- Current AHT20 temperature and relative humidity
- Current BMP280 temperature and atmospheric pressure
- Historical charts
- Configurable history range
- Stale-data indication
- SQLite database integration

## Architecture

Raspberry Pi sensors
    -> sensor-monitor
    -> MQTT
    -> Ubuntu sensor logger
    -> SQLite database
    -> Flask dashboard
    -> Web browser

## Requirements

- Python 3
- Flask
- SQLite database created by sensor_logger.py
- Internet access in the browser for Chart.js

## Running

Start the sensor logger first:

    cd ~/sensor-logger
    python3 sensor_logger.py

In another terminal, start the dashboard from this directory:

    python3 dashboard.py

Open:

    http://127.0.0.1:5000

The dashboard reads the existing SQLite database. It does not communicate
directly with the sensors or publish MQTT messages.

## Security

The development server listens on localhost only. Do not expose it directly
to an untrusted network without appropriate security controls.
