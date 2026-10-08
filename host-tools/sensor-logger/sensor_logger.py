#!/usr/bin/env python3

import json
import sqlite3
import signal
import sys
import time
from datetime import datetime, timezone

import paho.mqtt.client as mqtt


MQTT_BROKER = "10.42.0.1"
MQTT_PORT = 1883

MQTT_USERNAME = "sensorpi"
MQTT_PASSWORD = "YOUR_PASSWORD"

MQTT_TOPIC = "sensors/environment"

#DATABASE = "/var/lib/sensor-logger/sensors.db"
DATABASE = "/home/steve/sensor-logger/sensors.db"
running = True


def signal_handler(signum, frame):
    global running

    print("Shutting down...")
    running = False


def create_database():
    connection = sqlite3.connect(DATABASE)

    cursor = connection.cursor()

    cursor.execute(
        """
        CREATE TABLE IF NOT EXISTS readings (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp TEXT NOT NULL,
            aht20_temperature_c REAL NOT NULL,
            aht20_humidity_percent REAL NOT NULL,
            bmp280_temperature_c REAL NOT NULL,
            bmp280_pressure_hpa REAL NOT NULL
        )
        """
    )

    connection.commit()
    connection.close()


def store_reading(data):
    try:
        timestamp = data["timestamp"]

        aht20 = data["aht20"]
        bmp280 = data["bmp280"]

        aht20_temperature = float(aht20["temperature_c"])
        aht20_humidity = float(aht20["humidity_percent"])

        bmp280_temperature = float(bmp280["temperature_c"])
        bmp280_pressure = float(bmp280["pressure_hpa"])

    except (KeyError, TypeError, ValueError) as error:
        print(f"Invalid sensor message: {error}")
        return

    connection = sqlite3.connect(DATABASE)

    cursor = connection.cursor()

    cursor.execute(
        """
        INSERT INTO readings (
            timestamp,
            aht20_temperature_c,
            aht20_humidity_percent,
            bmp280_temperature_c,
            bmp280_pressure_hpa
        )
        VALUES (?, ?, ?, ?, ?)
        """,
        (
            timestamp,
            aht20_temperature,
            aht20_humidity,
            bmp280_temperature,
            bmp280_pressure,
        ),
    )

    connection.commit()
    connection.close()

    print(
        f"Stored: {timestamp} | "
        f"AHT20 {aht20_temperature:.2f} C, "
        f"{aht20_humidity:.2f} % RH | "
        f"BMP280 {bmp280_temperature:.2f} C, "
        f"{bmp280_pressure:.2f} hPa"
    )


def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("Connected to MQTT broker")

        result = client.subscribe(MQTT_TOPIC, qos=1)

        if result[0] != mqtt.MQTT_ERR_SUCCESS:
            print(f"MQTT subscribe failed: {result}")
        else:
            print(f"Subscribed to {MQTT_TOPIC}")

    else:
        print(f"MQTT connection failed: {rc}")


def on_disconnect(client, userdata, rc):
    print(f"MQTT disconnected: {rc}")


def on_message(client, userdata, message):
    if message.topic != MQTT_TOPIC:
        return

    try:
        payload = message.payload.decode("utf-8")
        data = json.loads(payload)
        store_reading(data)

    except UnicodeDecodeError as error:
        print(f"Invalid MQTT payload: {error}")

    except json.JSONDecodeError as error:
        print(f"Invalid JSON: {error}")


def main():
    global running

    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)

    print("Sensor logger starting")

    print(f"MQTT broker: {MQTT_BROKER}:{MQTT_PORT}")
    print(f"MQTT topic:  {MQTT_TOPIC}")
    print(f"Database:    {DATABASE}")

    create_database()

    client = mqtt.Client(
        client_id="sensor-logger",
    )

    client.username_pw_set(
        MQTT_USERNAME,
        MQTT_PASSWORD,
    )

    client.on_connect = on_connect
    client.on_disconnect = on_disconnect
    client.on_message = on_message

    client.reconnect_delay_set(
        min_delay=1,
        max_delay=30,
    )

    print("Connecting to MQTT broker...")

    try:
        client.connect(
            MQTT_BROKER,
            MQTT_PORT,
            keepalive=60,
        )
    except Exception as error:
        print(f"Initial MQTT connection failed: {error}")

    client.loop_start()

    try:
        while running:
            time.sleep(1)

    except KeyboardInterrupt:
        running = False

    print("Stopping MQTT client")

    client.loop_stop()

    try:
        client.disconnect()
    except Exception:
        pass

    print("Sensor logger stopped")


if __name__ == "__main__":
    main()
