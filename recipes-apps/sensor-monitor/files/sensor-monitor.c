#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define IIO_BASE "/sys/bus/iio/devices"
#define CONFIG_FILE "/etc/sensor-monitor.conf"

#define DEFAULT_INTERVAL_SECONDS 5
#define MIN_INTERVAL_SECONDS 1
#define MAX_INTERVAL_SECONDS 3600

#define SENSOR_RETRY_DELAY_SECONDS 2

static volatile sig_atomic_t running = 1;

static void handle_signal(int signal)
{
    (void)signal;
    running = 0;
}

static int find_iio_device(const char *name,
                           char *path,
                           size_t path_size)
{
    DIR *dir;
    struct dirent *entry;
    char name_path[256];
    char device_name[64];
    FILE *fp;

    dir = opendir(IIO_BASE);

    if (!dir) {
        fprintf(stderr,
                "Failed to open %s: %s\n",
                IIO_BASE,
                strerror(errno));
        return -1;
    }

    while ((entry = readdir(dir)) != NULL) {

        if (strncmp(entry->d_name,
                    "iio:device",
                    10) != 0)
            continue;

        snprintf(name_path,
                 sizeof(name_path),
                 "%s/%s/name",
                 IIO_BASE,
                 entry->d_name);

        fp = fopen(name_path, "r");

        if (!fp)
            continue;

        if (fgets(device_name,
                  sizeof(device_name),
                  fp) != NULL) {

            device_name[strcspn(device_name, "\n")] = '\0';

            if (strcmp(device_name, name) == 0) {

                snprintf(path,
                         path_size,
                         "%s/%s",
                         IIO_BASE,
                         entry->d_name);

                fclose(fp);
                closedir(dir);

                return 0;
            }
        }

        fclose(fp);
    }

    closedir(dir);

    fprintf(stderr,
            "IIO device '%s' not found\n",
            name);

    return -1;
}

static int read_value(const char *path,
                      double *value)
{
    FILE *fp;

    fp = fopen(path, "r");

    if (!fp) {
        fprintf(stderr,
                "Failed to open %s: %s\n",
                path,
                strerror(errno));
        return -1;
    }

    if (fscanf(fp, "%lf", value) != 1) {
        fprintf(stderr,
                "Failed to read %s\n",
                path);
        fclose(fp);
        return -1;
    }

    fclose(fp);

    return 0;
}

static int read_aht20(const char *device,
                      double *temperature,
                      double *humidity)
{
    char path[256];

    snprintf(path,
             sizeof(path),
             "%s/in_temp_input",
             device);

    if (read_value(path, temperature) < 0)
        return -1;

    snprintf(path,
             sizeof(path),
             "%s/in_humidityrelative_input",
             device);

    if (read_value(path, humidity) < 0)
        return -1;

    return 0;
}

static int read_bmp280(const char *device,
                       double *temperature,
                       double *pressure)
{
    char path[256];

    snprintf(path,
             sizeof(path),
             "%s/in_temp_input",
             device);

    if (read_value(path, temperature) < 0)
        return -1;

    snprintf(path,
             sizeof(path),
             "%s/in_pressure_input",
             device);

    if (read_value(path, pressure) < 0)
        return -1;

    return 0;
}

static int load_interval(void)
{
    FILE *fp;
    char line[128];
    int interval = DEFAULT_INTERVAL_SECONDS;

    fp = fopen(CONFIG_FILE, "r");

    if (!fp) {
        if (errno == ENOENT) {
            printf("Configuration file not found, "
                   "using default interval of %d seconds\n",
                   DEFAULT_INTERVAL_SECONDS);
            return DEFAULT_INTERVAL_SECONDS;
        }

        fprintf(stderr,
                "Failed to open %s: %s\n",
                CONFIG_FILE,
                strerror(errno));

        return DEFAULT_INTERVAL_SECONDS;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {

        int value;

        if (sscanf(line,
                   "INTERVAL_SECONDS=%d",
                   &value) == 1) {

            if (value >= MIN_INTERVAL_SECONDS &&
                value <= MAX_INTERVAL_SECONDS) {

                interval = value;
            } else {

                fprintf(stderr,
                        "Invalid interval %d in %s\n",
                        value,
                        CONFIG_FILE);

                fprintf(stderr,
                        "Using default interval of %d seconds\n",
                        DEFAULT_INTERVAL_SECONDS);

                interval = DEFAULT_INTERVAL_SECONDS;
            }

            break;
        }
    }

    fclose(fp);

    return interval;
}

static void print_timestamp(void)
{
    time_t now;
    struct tm tm_now;
    char timestamp[32];

    now = time(NULL);

    if (localtime_r(&now, &tm_now) == NULL)
        return;

    if (strftime(timestamp,
                 sizeof(timestamp),
                 "%Y-%m-%d %H:%M:%S",
                 &tm_now) == 0)
        return;

    printf("%s\n", timestamp);
}

static int sleep_interruptible(int seconds)
{
    int remaining = seconds;

    while (remaining > 0 && running) {
        remaining = sleep(remaining);
    }

    return running ? 0 : -1;
}

int main(void)
{
    char aht20_device[256];
    char bmp280_device[256];

    double aht20_temperature;
    double aht20_humidity;

    double bmp280_temperature;
    double bmp280_pressure;

    int interval;
    int retry;

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    printf("Sensor monitor started\n");

    interval = load_interval();

    printf("Sampling interval: %d seconds\n",
           interval);

    /*
     * Wait for the IIO devices to appear.
     *
     * This makes the application more tolerant of
     * sensors appearing slightly later during boot.
     */
    while (running) {

        if (find_iio_device("aht20",
                            aht20_device,
                            sizeof(aht20_device)) == 0)
            break;

        fprintf(stderr,
                "Waiting for AHT20 IIO device...\n");

        if (sleep_interruptible(SENSOR_RETRY_DELAY_SECONDS) < 0)
            break;
    }

    if (!running)
        goto stopped;

    while (running) {

        if (find_iio_device("bmp280",
                            bmp280_device,
                            sizeof(bmp280_device)) == 0)
            break;

        fprintf(stderr,
                "Waiting for BMP280 IIO device...\n");

        if (sleep_interruptible(SENSOR_RETRY_DELAY_SECONDS) < 0)
            break;
    }

    if (!running)
        goto stopped;

    printf("AHT20:  %s\n", aht20_device);
    printf("BMP280: %s\n", bmp280_device);
    printf("\n");

    while (running) {

        int success = 0;

        /*
         * Try the complete sensor read several times.
         *
         * A transient IIO failure should not cause the
         * entire monitoring service to terminate.
         */
        for (retry = 0; retry < 3 && running; retry++) {

            if (read_aht20(aht20_device,
                           &aht20_temperature,
                           &aht20_humidity) < 0) {

                fprintf(stderr,
                        "AHT20 read failed "
                        "(attempt %d/3)\n",
                        retry + 1);

                if (sleep_interruptible(
                        SENSOR_RETRY_DELAY_SECONDS) < 0)
                    break;

                continue;
            }

            if (read_bmp280(bmp280_device,
                            &bmp280_temperature,
                            &bmp280_pressure) < 0) {

                fprintf(stderr,
                        "BMP280 read failed "
                        "(attempt %d/3)\n",
                        retry + 1);

                if (sleep_interruptible(
                        SENSOR_RETRY_DELAY_SECONDS) < 0)
                    break;

                continue;
            }

            success = 1;
            break;
        }

        if (!running)
            break;

        if (!success) {

            fprintf(stderr,
                    "Sensor read failed after 3 attempts\n");

            /*
             * Re-discover the IIO devices in case a driver
             * was temporarily removed and re-registered.
             */
            find_iio_device("aht20",
                            aht20_device,
                            sizeof(aht20_device));

            find_iio_device("bmp280",
                            bmp280_device,
                            sizeof(bmp280_device));

            if (sleep_interruptible(
                    SENSOR_RETRY_DELAY_SECONDS) < 0)
                break;

            continue;
        }

        print_timestamp();

        printf("AHT20   Temperature: %6.2f °C   "
               "Humidity: %6.2f %%\n",
               aht20_temperature / 1000.0,
               aht20_humidity / 1000.0);

        printf("BMP280  Temperature: %6.2f °C   "
               "Pressure: %7.2f hPa\n",
               bmp280_temperature / 1000.0,
               bmp280_pressure * 10.0);

        printf("\n");

        fflush(stdout);

        if (sleep_interruptible(interval) < 0)
            break;
    }

stopped:

    printf("Sensor monitor stopped\n");

    return 0;
}
