#!/usr/bin/env python3

import sqlite3
from datetime import datetime, timezone
from pathlib import Path

from flask import Flask, jsonify, render_template, request


app = Flask(__name__)

DATABASE = Path.home() / "sensor-logger" / "sensors.db"

DEFAULT_LIMIT = 120
MAX_LIMIT = 2000
STALE_AFTER_SECONDS = 20


def get_connection():
    connection = sqlite3.connect(DATABASE, timeout=5)
    connection.row_factory = sqlite3.Row
    return connection


def get_readings(limit=DEFAULT_LIMIT):
    with get_connection() as connection:
        rows = connection.execute(
            """
            SELECT
                id,
                timestamp,
                aht20_temperature_c,
                aht20_humidity_percent,
                bmp280_temperature_c,
                bmp280_pressure_hpa
            FROM readings
            ORDER BY id DESC
            LIMIT ?
            """,
            (limit,),
        ).fetchall()

    # Return chronological order for plotting.
    return [dict(row) for row in reversed(rows)]


def get_status(latest):
    if latest is None:
        return {
            "state": "no-data",
            "message": "No readings in the database",
            "age_seconds": None,
        }

    try:
        timestamp = datetime.fromisoformat(
            latest["timestamp"].replace("Z", "+00:00")
        )

        if timestamp.tzinfo is None:
            timestamp = timestamp.replace(tzinfo=timezone.utc)

        age = (
            datetime.now(timezone.utc) - timestamp.astimezone(timezone.utc)
        ).total_seconds()

    except (ValueError, TypeError):
        return {
            "state": "unknown",
            "message": "Latest timestamp is invalid",
            "age_seconds": None,
        }

    age = max(0, int(age))

    if age > STALE_AFTER_SECONDS:
        state = "stale"
        message = "No recent sensor readings"
    else:
        state = "online"
        message = "Receiving recent sensor readings"

    return {
        "state": state,
        "message": message,
        "age_seconds": age,
    }


@app.route("/")
def index():
    return render_template("index.html")


@app.route("/api/readings")
def api_readings():
    try:
        limit = int(request.args.get("limit", DEFAULT_LIMIT))
    except ValueError:
        return jsonify({"error": "limit must be an integer"}), 400

    if limit < 1 or limit > MAX_LIMIT:
        return jsonify({
            "error": f"limit must be between 1 and {MAX_LIMIT}"
        }), 400

    try:
        readings = get_readings(limit)

    except sqlite3.Error as error:
        app.logger.error("Database query failed: %s", error)
        return jsonify({"error": "Could not read sensor database"}), 500

    latest = readings[-1] if readings else None

    return jsonify({
        "readings": readings,
        "latest": latest,
        "status": get_status(latest),
    })


@app.route("/api/history")
def api_history():
    try:
        hours = float(request.args.get("hours", "1"))
    except ValueError:
        return jsonify({"error": "hours must be numeric"}), 400

    if not 0 < hours <= 168:
        return jsonify({
            "error": "hours must be greater than 0 and at most 168"
        }), 400

    try:
        with get_connection() as connection:
            rows = connection.execute(
                """
                SELECT
                    timestamp,
                    aht20_temperature_c,
                    aht20_humidity_percent,
                    bmp280_temperature_c,
                    bmp280_pressure_hpa
                FROM readings
                WHERE timestamp >= strftime(
                    '%Y-%m-%dT%H:%M:%SZ',
                    'now',
                    ?
                )
                ORDER BY id ASC
                """,
                (f"-{hours} hours",),
            ).fetchall()

        return jsonify({"readings": [dict(row) for row in rows]})

    except sqlite3.Error as error:
        app.logger.error("History query failed: %s", error)
        return jsonify({"error": "Could not read sensor history"}), 500


if __name__ == "__main__":
    if not DATABASE.exists():
        raise SystemExit(
            f"Database not found: {DATABASE}\n"
            "Run sensor_logger.py first to create the database."
        )

    print(f"Database: {DATABASE}")
    print("Dashboard: http://127.0.0.1:5000")

    app.run(
        host="127.0.0.1",
        port=5000,
        debug=False,
    )
