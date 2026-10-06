#include <stdio.h>

#define IIO_DEVICE "/sys/bus/iio/devices/iio:device0"

static int read_value(const char *path, double *value)
{
    FILE *fp = fopen(path, "r");

    if (!fp) {
        perror(path);
        return -1;
    }

    if (fscanf(fp, "%lf", value) != 1) {
        fprintf(stderr, "Failed to read %s\n", path);
        fclose(fp);
        return -1;
    }

    fclose(fp);
    return 0;
}

int main(void)
{
    char path[256];
    double temperature;
    double pressure;

    snprintf(path, sizeof(path),
             "%s/in_temp_input", IIO_DEVICE);

    if (read_value(path, &temperature) < 0)
        return 1;

    snprintf(path, sizeof(path),
             "%s/in_pressure_input", IIO_DEVICE);

    if (read_value(path, &pressure) < 0)
        return 1;

    printf("BMP280 via IIO\n");
    printf("Temperature: %.3f °C\n", temperature / 1000.0);
    printf("Pressure:    %.2f hPa\n", pressure * 10.0);

    return 0;
}
