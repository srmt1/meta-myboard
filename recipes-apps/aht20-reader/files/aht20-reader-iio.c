#include <dirent.h>
#include <stdio.h>
#include <string.h>

#define IIO_BASE "/sys/bus/iio/devices"

static int find_iio_device(const char *name, char *path, size_t path_size)
{
    DIR *dir;
    struct dirent *entry;
    char name_path[256];
    char device_name[64];
    FILE *fp;

    dir = opendir(IIO_BASE);

    if (!dir) {
        perror(IIO_BASE);
        return -1;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "iio:device", 10) != 0)
            continue;

        snprintf(name_path, sizeof(name_path),
                 "%s/%s/name",
                 IIO_BASE, entry->d_name);

        fp = fopen(name_path, "r");

        if (!fp)
            continue;

        if (fgets(device_name, sizeof(device_name), fp) != NULL) {
            device_name[strcspn(device_name, "\n")] = '\0';

            if (strcmp(device_name, name) == 0) {
                snprintf(path, path_size,
                         "%s/%s",
                         IIO_BASE, entry->d_name);

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
    char device[256];
    char path[256];
    double temperature;
    double humidity;

    if (find_iio_device("aht20",
                        device,
                        sizeof(device)) < 0)
        return 1;

    snprintf(path, sizeof(path),
             "%s/in_temp_input",
             device);

    if (read_value(path, &temperature) < 0)
        return 1;

    snprintf(path, sizeof(path),
             "%s/in_humidityrelative_input",
             device);

    if (read_value(path, &humidity) < 0)
        return 1;

    printf("AHT20 via IIO\n");
    printf("Temperature: %.3f °C\n",
           temperature / 1000.0);

    printf("Humidity:    %.3f %%\n",
           humidity / 1000.0);

    return 0;
}
