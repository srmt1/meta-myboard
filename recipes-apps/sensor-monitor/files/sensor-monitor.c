#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>

#include <mosquitto.h>

#define IIO_BASE "/sys/bus/iio/devices"
#define CONFIG_FILE "/etc/sensor-monitor.conf"

#define DEFAULT_INTERVAL_SECONDS 5
#define MIN_INTERVAL_SECONDS 1
#define MAX_INTERVAL_SECONDS 3600

#define SENSOR_RETRY_DELAY_SECONDS 2
#define MQTT_RECONNECT_DELAY_SECONDS 5
#define MQTT_KEEPALIVE_SECONDS 60

#define MQTT_STATUS_TOPIC "sensors/status"
#define MQTT_STATUS_ONLINE "online"
#define MQTT_STATUS_OFFLINE "offline"

static volatile sig_atomic_t running = 1;

static int interval_seconds = DEFAULT_INTERVAL_SECONDS;

static char mqtt_broker[256] = "10.42.0.1";
static int mqtt_port = 1883;
static char mqtt_username[128] = "";
static char mqtt_password[128] = "";
static char mqtt_topic[256] = "sensors/environment";
static char mqtt_command_topic[256] = "sensors/command";

static struct mosquitto *mosq = NULL;
static bool mqtt_connected = false;
static time_t mqtt_last_reconnect_attempt = 0;


/* ------------------------------------------------------------------------- */
/* Signal handling                                                          */
/* ------------------------------------------------------------------------- */

static void handle_signal(int signal)
{
    (void)signal;
    running = 0;
}


/* ------------------------------------------------------------------------- */
/* Configuration                                                            */
/* ------------------------------------------------------------------------- */

static void trim_whitespace(char *str)
{
    char *start;
    char *end;

    start = str;

    while (*start == ' ' || *start == '\t' ||
           *start == '\r' || *start == '\n') {
        start++;
    }

    if (start != str)
        memmove(str, start, strlen(start) + 1);

    end = str + strlen(str);

    while (end > str &&
           (end[-1] == ' ' || end[-1] == '\t' ||
            end[-1] == '\r' || end[-1] == '\n')) {
        end--;
    }

    *end = '\0';
}


static void load_config(void)
{
    FILE *file;
    char line[512];

    file = fopen(CONFIG_FILE, "r");

    if (file == NULL) {
        printf("Using default configuration\n");
        return;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        char *equals;
        char *key;
        char *value;

        trim_whitespace(line);

        if (line[0] == '\0' || line[0] == '#')
            continue;

        equals = strchr(line, '=');

        if (equals == NULL)
            continue;

        *equals = '\0';

        key = line;
        value = equals + 1;

        trim_whitespace(key);
        trim_whitespace(value);

        if (strcmp(key, "INTERVAL_SECONDS") == 0) {
            int value_int = atoi(value);

            if (value_int >= MIN_INTERVAL_SECONDS &&
                value_int <= MAX_INTERVAL_SECONDS) {
                interval_seconds = value_int;
            } else {
                fprintf(stderr,
                        "Invalid INTERVAL_SECONDS=%s, using %d\n",
                        value,
                        DEFAULT_INTERVAL_SECONDS);

                interval_seconds = DEFAULT_INTERVAL_SECONDS;
            }
        }

        else if (strcmp(key, "MQTT_BROKER") == 0) {
            snprintf(mqtt_broker,
                     sizeof(mqtt_broker),
                     "%s",
                     value);
        }

        else if (strcmp(key, "MQTT_PORT") == 0) {
            int value_int = atoi(value);

            if (value_int > 0 && value_int <= 65535)
                mqtt_port = value_int;
        }

        else if (strcmp(key, "MQTT_USERNAME") == 0) {
            snprintf(mqtt_username,
                     sizeof(mqtt_username),
                     "%s",
                     value);
        }

        else if (strcmp(key, "MQTT_PASSWORD") == 0) {
            snprintf(mqtt_password,
                     sizeof(mqtt_password),
                     "%s",
                     value);
        }

        else if (strcmp(key, "MQTT_TOPIC") == 0) {
            snprintf(mqtt_topic,
                     sizeof(mqtt_topic),
                     "%s",
                     value);
        }

        else if (strcmp(key, "MQTT_COMMAND_TOPIC") == 0) {
            snprintf(mqtt_command_topic,
                     sizeof(mqtt_command_topic),
                     "%s",
                     value);
        }
    }

    fclose(file);
}


/* ------------------------------------------------------------------------- */
/* IIO device discovery                                                     */
/* ------------------------------------------------------------------------- */

static int find_iio_device(const char *device_name,
                           char *device_path,
                           size_t device_path_size)
{
    DIR *dir;
    struct dirent *entry;

    dir = opendir(IIO_BASE);

    if (dir == NULL) {
        perror("opendir IIO");
        return -1;
    }

    while ((entry = readdir(dir)) != NULL) {
        char name_path[512];
        char name[256];
        FILE *file;

        if (strncmp(entry->d_name, "iio:device", 10) != 0)
            continue;

        snprintf(name_path,
                 sizeof(name_path),
                 "%s/%s/name",
                 IIO_BASE,
                 entry->d_name);

        file = fopen(name_path, "r");

        if (file == NULL)
            continue;

        if (fgets(name, sizeof(name), file) != NULL) {
            trim_whitespace(name);

            if (strcmp(name, device_name) == 0) {
                snprintf(device_path,
                         device_path_size,
                         "%s/%s",
                         IIO_BASE,
                         entry->d_name);

                fclose(file);
                closedir(dir);

                return 0;
            }
        }

        fclose(file);
    }

    closedir(dir);

    return -1;
}


/* ------------------------------------------------------------------------- */
/* IIO value reading                                                        */
/* ------------------------------------------------------------------------- */

static int read_iio_value(const char *device_path,
                          const char *filename,
                          double *value)
{
    char path[512];
    FILE *file;
    char buffer[128];
    char *end;
    double result;

    snprintf(path,
             sizeof(path),
             "%s/%s",
             device_path,
             filename);

    file = fopen(path, "r");

    if (file == NULL)
        return -1;

    if (fgets(buffer, sizeof(buffer), file) == NULL) {
        fclose(file);
        return -1;
    }

    fclose(file);

    errno = 0;

    result = strtod(buffer, &end);

    if (errno != 0 || end == buffer)
        return -1;

    *value = result;

    return 0;
}


/* ------------------------------------------------------------------------- */
/* Sensor reading                                                           */
/* ------------------------------------------------------------------------- */

static int read_sensors(double *aht20_temperature,
                        double *aht20_humidity,
                        double *bmp280_temperature,
                        double *bmp280_pressure)
{
    char aht20_path[512];
    char bmp280_path[512];

    double value;

    /*
     * Discover the devices by their IIO name rather than assuming
     * iio:device0 / iio:device1.
     */
    if (find_iio_device("aht20",
                        aht20_path,
                        sizeof(aht20_path)) != 0) {
        fprintf(stderr, "AHT20 IIO device not found\n");
        return -1;
    }

    if (find_iio_device("bmp280",
                        bmp280_path,
                        sizeof(bmp280_path)) != 0) {
        fprintf(stderr, "BMP280 IIO device not found\n");
        return -1;
    }

    /*
     * AHT20 IIO values are in milli-degrees C and milli-percent RH.
     */
    if (read_iio_value(aht20_path,
                       "in_temp_input",
                       &value) != 0) {
        fprintf(stderr, "Failed to read AHT20 temperature\n");
        return -1;
    }

    *aht20_temperature = value / 1000.0;

    if (read_iio_value(aht20_path,
                       "in_humidityrelative_input",
                       &value) != 0) {
        fprintf(stderr, "Failed to read AHT20 humidity\n");
        return -1;
    }

    *aht20_humidity = value / 1000.0;


    /*
     * BMP280 temperature is milli-degrees C.
     */
    if (read_iio_value(bmp280_path,
                       "in_temp_input",
                       &value) != 0) {
        fprintf(stderr, "Failed to read BMP280 temperature\n");
        return -1;
    }

    *bmp280_temperature = value / 1000.0;


    /*
     * The BMP280 IIO pressure value is in kPa.
     * Convert kPa -> hPa by multiplying by 10.
     */
    if (read_iio_value(bmp280_path,
                       "in_pressure_input",
                       &value) != 0) {
        fprintf(stderr, "Failed to read BMP280 pressure\n");
        return -1;
    }

    *bmp280_pressure = value * 10.0;

    return 0;
}


/* ------------------------------------------------------------------------- */
/* MQTT                                                                     */
/* ------------------------------------------------------------------------- */

static void mqtt_publish_status(const char *status)
{
    int rc;

    if (!mqtt_connected)
        return;

    rc = mosquitto_publish(
        mosq,
        NULL,
        MQTT_STATUS_TOPIC,
        (int)strlen(status),
        status,
        1,
        true);

    if (rc != MOSQ_ERR_SUCCESS) {
        fprintf(stderr,
                "Failed to publish MQTT status: %s\n",
                mosquitto_strerror(rc));
    } else {
        printf("MQTT status: %s\n", status);
    }
}


static void mqtt_publish_json(double aht20_temperature,
                              double aht20_humidity,
                              double bmp280_temperature,
                              double bmp280_pressure)
{
    char json[1024];
    char timestamp[64];

    time_t now;
    struct tm tm_utc;

    int rc;

    now = time(NULL);

    if (gmtime_r(&now, &tm_utc) == NULL) {
        fprintf(stderr, "Failed to obtain UTC time\n");
        return;
    }

    if (strftime(timestamp,
                 sizeof(timestamp),
                 "%Y-%m-%dT%H:%M:%SZ",
                 &tm_utc) == 0) {
        fprintf(stderr, "Failed to format timestamp\n");
        return;
    }

    snprintf(
        json,
        sizeof(json),
        "{\"timestamp\":\"%s\","
        "\"aht20\":{\"temperature_c\":%.2f,"
        "\"humidity_percent\":%.2f},"
        "\"bmp280\":{\"temperature_c\":%.2f,"
        "\"pressure_hpa\":%.2f}}",
        timestamp,
        aht20_temperature,
        aht20_humidity,
        bmp280_temperature,
        bmp280_pressure);

    if (!mqtt_connected)
        return;

    rc = mosquitto_publish(
        mosq,
        NULL,
        mqtt_topic,
        (int)strlen(json),
        json,
        1,
        true);

    if (rc != MOSQ_ERR_SUCCESS) {
        fprintf(stderr,
                "MQTT publish failed: %s\n",
                mosquitto_strerror(rc));
    } else {
        printf("MQTT published: %s\n", json);
    }
}


/* ------------------------------------------------------------------------- */
/* MQTT callbacks                                                           */
/* ------------------------------------------------------------------------- */

static void mqtt_connect_callback(struct mosquitto *client,
                                   void *userdata,
                                   int rc)
{
    (void)client;
    (void)userdata;

    if (rc != 0) {
        fprintf(stderr,
                "MQTT connection failed: %s\n",
                mosquitto_connack_string(rc));

        mqtt_connected = false;
        return;
    }

    mqtt_connected = true;

    printf("MQTT connected to %s:%d\n",
           mqtt_broker,
           mqtt_port);

    /*
     * Subscribe to the command topic after every successful connection.
     * This is important because subscriptions are lost when the MQTT
     * connection is lost.
     */
    rc = mosquitto_subscribe(
        mosq,
        NULL,
        mqtt_command_topic,
        1);

    if (rc != MOSQ_ERR_SUCCESS) {
        fprintf(stderr,
                "MQTT subscribe failed: %s\n",
                mosquitto_strerror(rc));
    } else {
        printf("MQTT subscribed: %s\n",
               mqtt_command_topic);
    }

    /*
     * Publish "online" as a retained message.
     *
     * The Last Will was configured as "offline", so if this client
     * disappears unexpectedly the broker will publish "offline".
     */
    mqtt_publish_status(MQTT_STATUS_ONLINE);
}


static void mqtt_disconnect_callback(struct mosquitto *client,
                                     void *userdata,
                                     int rc)
{
    (void)client;
    (void)userdata;

    mqtt_connected = false;

    if (rc != 0) {
        printf("MQTT connection lost unexpectedly\n");
    } else {
        printf("MQTT disconnected\n");
    }
}


static void mqtt_message_callback(struct mosquitto *client,
                                  void *userdata,
                                  const struct mosquitto_message *message)
{
    char command[256];
    int new_interval;

    (void)client;
    (void)userdata;

    if (message == NULL ||
        message->payload == NULL ||
        message->payloadlen <= 0) {
        return;
    }

    if (strcmp(message->topic, mqtt_command_topic) != 0)
        return;

    if (message->payloadlen >= (int)sizeof(command)) {
        fprintf(stderr, "MQTT command too long\n");
        return;
    }

    memcpy(command,
           message->payload,
           message->payloadlen);

    command[message->payloadlen] = '\0';

    trim_whitespace(command);

    printf("MQTT command received: %s\n", command);

    if (sscanf(command,
               "interval %d",
               &new_interval) == 1) {

        if (new_interval >= MIN_INTERVAL_SECONDS &&
            new_interval <= MAX_INTERVAL_SECONDS) {

            interval_seconds = new_interval;

            printf("MQTT command: sampling interval changed to %d seconds\n",
                   interval_seconds);
        } else {
            fprintf(stderr,
                    "MQTT command: interval must be between %d and %d seconds\n",
                    MIN_INTERVAL_SECONDS,
                    MAX_INTERVAL_SECONDS);
        }
    } else {
        fprintf(stderr,
                "Unknown MQTT command: %s\n",
                command);
    }
}


/* ------------------------------------------------------------------------- */
/* MQTT setup                                                                */
/* ------------------------------------------------------------------------- */

static int mqtt_setup(void)
{
    int rc;

    rc = mosquitto_lib_init();

    if (rc != MOSQ_ERR_SUCCESS) {
        fprintf(stderr,
                "mosquitto_lib_init failed: %s\n",
                mosquitto_strerror(rc));
        return -1;
    }

    mosq = mosquitto_new(
        "sensor-monitor",
        true,
        NULL);

    if (mosq == NULL) {
        fprintf(stderr, "Failed to create MQTT client\n");
        mosquitto_lib_cleanup();
        return -1;
    }

    /*
     * Last Will and Testament.
     *
     * If the MQTT connection disappears unexpectedly, the broker
     * publishes:
     *
     *     sensors/status = offline
     *
     * The message is retained.
     */
    rc = mosquitto_will_set(
        mosq,
        MQTT_STATUS_TOPIC,
        (int)strlen(MQTT_STATUS_OFFLINE),
        MQTT_STATUS_OFFLINE,
        1,
        true);

    if (rc != MOSQ_ERR_SUCCESS) {
        fprintf(stderr,
                "Failed to configure MQTT Last Will: %s\n",
                mosquitto_strerror(rc));

        mosquitto_destroy(mosq);
        mosq = NULL;

        mosquitto_lib_cleanup();

        return -1;
    }

    mosquitto_connect_callback_set(
        mosq,
        mqtt_connect_callback);

    mosquitto_disconnect_callback_set(
        mosq,
        mqtt_disconnect_callback);

    mosquitto_message_callback_set(
        mosq,
        mqtt_message_callback);

    /*
     * Use username/password authentication if configured.
     */
    if (mqtt_username[0] != '\0') {
        rc = mosquitto_username_pw_set(
            mosq,
            mqtt_username,
            mqtt_password);

        if (rc != MOSQ_ERR_SUCCESS) {
            fprintf(stderr,
                    "Failed to configure MQTT username/password: %s\n",
                    mosquitto_strerror(rc));

            mosquitto_destroy(mosq);
            mosq = NULL;

            mosquitto_lib_cleanup();

            return -1;
        }
    }

    /*
     * Configure automatic reconnect delay.
     *
     * The MQTT keepalive is supplied directly to
     * mosquitto_connect_async() below.
     */
    mosquitto_reconnect_delay_set(
        mosq,
        MQTT_RECONNECT_DELAY_SECONDS,
        MQTT_RECONNECT_DELAY_SECONDS,
        false);

    /*
     * Start the asynchronous connection.
     */
    rc = mosquitto_connect_async(
        mosq,
        mqtt_broker,
        mqtt_port,
        MQTT_KEEPALIVE_SECONDS);

    if (rc != MOSQ_ERR_SUCCESS) {
        fprintf(stderr,
                "Initial MQTT connection failed: %s\n",
                mosquitto_strerror(rc));

        /*
         * This is non-fatal. Sensor monitoring should continue even
         * when the MQTT broker is unavailable.
         */
        mqtt_connected = false;
    }

    mqtt_last_reconnect_attempt = time(NULL);

    return 0;
}


/* ------------------------------------------------------------------------- */
/* MQTT service                                                              */
/* ------------------------------------------------------------------------- */

static void mqtt_service(void)
{
    int rc;
    time_t now;

    if (mosq == NULL)
        return;

    /*
     * Process MQTT network traffic.
     */
    rc = mosquitto_loop(
        mosq,
        0,
        1);

    if (rc != MOSQ_ERR_SUCCESS) {
        mqtt_connected = false;
    }

    /*
     * If disconnected, periodically request another asynchronous
     * connection.
     */
    if (!mqtt_connected) {
        now = time(NULL);

        if (now - mqtt_last_reconnect_attempt >=
            MQTT_RECONNECT_DELAY_SECONDS) {

            mqtt_last_reconnect_attempt = now;

            printf("MQTT reconnect attempt\n");

            rc = mosquitto_reconnect_async(mosq);

            if (rc != MOSQ_ERR_SUCCESS) {
                fprintf(stderr,
                        "MQTT reconnect failed: %s\n",
                        mosquitto_strerror(rc));
            }
        }
    }
}


/* ------------------------------------------------------------------------- */
/* MQTT cleanup                                                              */
/* ------------------------------------------------------------------------- */

static void mqtt_cleanup(void)
{
    if (mosq != NULL) {
        /*
         * A normal mosquitto_disconnect() means the broker will NOT
         * publish the Last Will.
         *
         * This is intentional: a normal shutdown is not an unexpected
         * failure.
         */
        if (mqtt_connected) {
            mosquitto_publish(
                mosq,
                NULL,
                MQTT_STATUS_TOPIC,
                (int)strlen(MQTT_STATUS_OFFLINE),
                MQTT_STATUS_OFFLINE,
                1,
                true);

            mosquitto_disconnect(mosq);
        }

        mosquitto_destroy(mosq);
        mosq = NULL;
    }

    mosquitto_lib_cleanup();
}


/* ------------------------------------------------------------------------- */
/* Main                                                                     */
/* ------------------------------------------------------------------------- */

int main(void)
{
    double aht20_temperature;
    double aht20_humidity;
    double bmp280_temperature;
    double bmp280_pressure;

    int failed_reads = 0;

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    printf("Sensor monitor starting\n");

    load_config();

    printf("Configuration:\n");
    printf("  Sampling interval: %d seconds\n",
           interval_seconds);
    printf("  MQTT broker:       %s:%d\n",
           mqtt_broker,
           mqtt_port);
    printf("  MQTT topic:        %s\n",
           mqtt_topic);
    printf("  MQTT command:      %s\n",
           mqtt_command_topic);
    printf("  MQTT status:       %s\n",
           MQTT_STATUS_TOPIC);

    /*
     * Give the IIO devices some time to appear after boot.
     */
    for (int attempt = 0; attempt < 30 && running; attempt++) {
        char path[512];

        if (find_iio_device("aht20",
                            path,
                            sizeof(path)) == 0 &&
            find_iio_device("bmp280",
                            path,
                            sizeof(path)) == 0) {
            break;
        }

        if (attempt == 29) {
            fprintf(stderr,
                    "Timed out waiting for IIO sensor devices\n");
        }

        sleep(1);
    }

    /*
     * MQTT is deliberately non-fatal.
     *
     * The sensor monitor continues to operate if the broker is
     * unavailable.
     */
    if (mqtt_setup() != 0) {
        fprintf(stderr,
                "MQTT setup failed; continuing without MQTT\n");
    }

    while (running) {
        time_t sample_start;

        sample_start = time(NULL);

        /*
         * Keep MQTT traffic moving while waiting for the next sample.
         */
        mqtt_service();

        if (read_sensors(
                &aht20_temperature,
                &aht20_humidity,
                &bmp280_temperature,
                &bmp280_pressure) == 0) {

            failed_reads = 0;

            printf(
                "AHT20: %.2f C, %.2f %% RH | "
                "BMP280: %.2f C, %.2f hPa\n",
                aht20_temperature,
                aht20_humidity,
                bmp280_temperature,
                bmp280_pressure);

            mqtt_publish_json(
                aht20_temperature,
                aht20_humidity,
                bmp280_temperature,
                bmp280_pressure);
        } else {
            failed_reads++;

            fprintf(stderr,
                    "Sensor read failed (attempt %d)\n",
                    failed_reads);

            /*
             * After repeated failures, simply rediscover the IIO
             * devices on the next iteration.
             */
        }

        /*
         * Wait for the configured interval while continuing to service
         * MQTT traffic.
         *
         * Use a monotonic clock so the interval is not affected by
         * changes to the system wall clock.
         */
        {
            struct timespec wait_start;
            struct timespec now;
            double elapsed_seconds;

            clock_gettime(CLOCK_MONOTONIC, &wait_start);

            do {
                mqtt_service();

                usleep(100000);

                clock_gettime(CLOCK_MONOTONIC, &now);

                elapsed_seconds =
                    (double)(now.tv_sec - wait_start.tv_sec) +
                    (double)(now.tv_nsec - wait_start.tv_nsec) / 1000000000.0;

            } while (running &&
                     elapsed_seconds < interval_seconds);
        }

        /*
         * Prevent an unused-variable warning with some compiler
         * configurations while retaining the sample-start timestamp
         * for future timing extensions.
         */
        (void)sample_start;
    }

    printf("Sensor monitor shutting down\n");

    mqtt_cleanup();

    return 0;
}
