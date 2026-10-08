#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <errno.h>
#include <string.h>

#define I2C_BUS     "/dev/i2c-1"
#define AHT20_ADDR  0x38

int main(void)
{
    int fd = -1;

    uint8_t command[3] = {0xAC, 0x33, 0x00};
    uint8_t data[7];

    uint32_t raw_humidity;
    uint32_t raw_temperature;

    double humidity;
    double temperature;

    /*
     * Open the I2C bus.
     */
    fd = open(I2C_BUS, O_RDWR);

    if (fd < 0) {
        fprintf(stderr,
                "Failed to open %s: %s\n",
                I2C_BUS, strerror(errno));
        close(fd);
    }

    /*
     * Select the AHT20 I2C address.
     */
    if (ioctl(fd, I2C_SLAVE, AHT20_ADDR) < 0) {
        fprintf(stderr,
                "Failed to select AHT20 at 0x%02X: %s\n",
                AHT20_ADDR, strerror(errno));
        close(fd);
    }

    /*
     * Trigger a measurement.
     */
    if (write(fd, command, sizeof(command)) != sizeof(command)) {
        fprintf(stderr,
                "Failed to send AHT20 measurement command: %s\n",
                strerror(errno));
        close(fd);
    }

    /*
     * Wait for the AHT20 measurement to complete.
     */
    usleep(80000);

    /*
     * Read the seven-byte response.
     */
    if (read(fd, data, sizeof(data)) != sizeof(data)) {
        fprintf(stderr,
                "Failed to read AHT20 response: %s\n",
                strerror(errno));
        close(fd);
    }

    /*
     * Extract the 20-bit humidity value.
     */
    raw_humidity =
        ((uint32_t)data[1] << 12) |
        ((uint32_t)data[2] << 4) |
        ((uint32_t)data[3] >> 4);

    /*
     * Extract the 20-bit temperature value.
     */
    raw_temperature =
        (((uint32_t)data[3] & 0x0F) << 16) |
        ((uint32_t)data[4] << 8) |
        data[5];

    /*
     * Convert raw values to physical units.
     */
    humidity =
        (raw_humidity * 100.0) / 1048576.0;

    temperature =
        (raw_temperature * 200.0) / 1048576.0 - 50.0;

    printf("AHT20\n");
    printf("Temperature: %.2f °C\n", temperature);
    printf("Humidity:    %.2f %%\n", humidity);

    close(fd);

}
