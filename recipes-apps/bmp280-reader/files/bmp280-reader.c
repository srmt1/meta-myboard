#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <errno.h>
#include <string.h>

#define I2C_BUS        "/dev/i2c-1"
#define BMP280_ADDR    0x77

#define REG_ID         0xD0
#define REG_CALIB      0x88
#define REG_CTRL_MEAS  0xF4
#define REG_STATUS     0xF3
#define REG_PRESS_MSB  0xF7
#define REG_TEMP_MSB   0xFA

static int read_registers(int fd, uint8_t reg, uint8_t *data, size_t len)
{
    if (write(fd, &reg, 1) != 1) {
        fprintf(stderr,
                "Failed to select register 0x%02X: %s\n",
                reg, strerror(errno));
        return -1;
    }

    if (read(fd, data, len) != (ssize_t)len) {
        fprintf(stderr,
                "I2C read failed: %s\n",
                strerror(errno));
        return -1;
    }

    return 0;
}

static int write_register(int fd, uint8_t reg, uint8_t value)
{
    uint8_t data[2];

    data[0] = reg;
    data[1] = value;

    if (write(fd, data, 2) != 2) {
        fprintf(stderr,
                "Failed to write register 0x%02X: %s\n",
                reg, strerror(errno));
        return -1;
    }

    return 0;
}

static uint16_t read_u16_le(const uint8_t *data)
{
    return (uint16_t)data[0] |
           ((uint16_t)data[1] << 8);
}

static int16_t read_s16_le(const uint8_t *data)
{
    return (int16_t)read_u16_le(data);
}

int main(void)
{
    int fd = -1;

    uint8_t id;
    uint8_t calib[24];

    uint8_t press_data[3];
    uint8_t temp_data[3];

    uint8_t status;

    uint32_t raw_pressure;
    uint32_t raw_temperature;

    uint16_t dig_T1;
    int16_t dig_T2;
    int16_t dig_T3;

    uint16_t dig_P1;
    int16_t dig_P2;
    int16_t dig_P3;
    int16_t dig_P4;
    int16_t dig_P5;
    int16_t dig_P6;
    int16_t dig_P7;
    int16_t dig_P8;
    int16_t dig_P9;

    int32_t t_fine;
    int32_t temperature;

    int64_t var1;
    int64_t var2;
    int64_t pressure;

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
     * Select the BMP280 I2C address.
     */
    if (ioctl(fd, I2C_SLAVE, BMP280_ADDR) < 0) {
        fprintf(stderr,
                "Failed to select BMP280 at 0x%02X: %s\n",
                BMP280_ADDR, strerror(errno));
        close(fd);
    }

    /*
     * Read chip ID.
     */
    if (read_registers(fd, REG_ID, &id, 1) < 0)
        close(fd);

    printf("BMP280\n");
    printf("Chip ID: 0x%02X\n", id);

    if (id != 0x58) {
        fprintf(stderr, "Unexpected chip ID\n");
        close(fd);
    }

    /*
     * Read calibration coefficients.
     */
    if (read_registers(fd, REG_CALIB, calib, sizeof(calib)) < 0)
        close(fd);

    dig_T1 = read_u16_le(&calib[0]);
    dig_T2 = read_s16_le(&calib[2]);
    dig_T3 = read_s16_le(&calib[4]);

    dig_P1 = read_u16_le(&calib[6]);
    dig_P2 = read_s16_le(&calib[8]);
    dig_P3 = read_s16_le(&calib[10]);
    dig_P4 = read_s16_le(&calib[12]);
    dig_P5 = read_s16_le(&calib[14]);
    dig_P6 = read_s16_le(&calib[16]);
    dig_P7 = read_s16_le(&calib[18]);
    dig_P8 = read_s16_le(&calib[20]);
    dig_P9 = read_s16_le(&calib[22]);

    printf("\nCalibration coefficients:\n");

    printf("dig_T1 = %u\n", dig_T1);
    printf("dig_T2 = %d\n", dig_T2);
    printf("dig_T3 = %d\n", dig_T3);

    printf("dig_P1 = %u\n", dig_P1);
    printf("dig_P2 = %d\n", dig_P2);
    printf("dig_P3 = %d\n", dig_P3);
    printf("dig_P4 = %d\n", dig_P4);
    printf("dig_P5 = %d\n", dig_P5);
    printf("dig_P6 = %d\n", dig_P6);
    printf("dig_P7 = %d\n", dig_P7);
    printf("dig_P8 = %d\n", dig_P8);
    printf("dig_P9 = %d\n", dig_P9);

    /*
     * CTRL_MEAS:
     *
     * temperature oversampling x1
     * pressure oversampling x1
     * normal mode
     *
     * 001 001 11 = 0x27
     */
    printf("\nStarting BMP280 measurements...\n");

    if (write_register(fd, REG_CTRL_MEAS, 0x27) < 0)
        close(fd);

    /*
     * Wait until the measurement is complete.
     *
     * STATUS bit 3:
     *   1 = measuring
     *   0 = measurement complete
     */
    do {
        if (read_registers(fd, REG_STATUS, &status, 1) < 0)
            close(fd);

        if (status & 0x08)
            usleep(5000);

    } while (status & 0x08);

    /*
     * Read raw pressure.
     */
    if (read_registers(fd, REG_PRESS_MSB, press_data, 3) < 0)
        close(fd);

    /*
     * Read raw temperature.
     */
    if (read_registers(fd, REG_TEMP_MSB, temp_data, 3) < 0)
        close(fd);

    /*
     * Convert the three bytes into the 20-bit ADC values.
     */
    raw_pressure =
        ((uint32_t)press_data[0] << 12) |
        ((uint32_t)press_data[1] << 4) |
        ((uint32_t)press_data[2] >> 4);

    raw_temperature =
        ((uint32_t)temp_data[0] << 12) |
        ((uint32_t)temp_data[1] << 4) |
        ((uint32_t)temp_data[2] >> 4);

    printf("\nRaw ADC values:\n");
    printf("raw_pressure    = %u\n", raw_pressure);
    printf("raw_temperature = %u\n", raw_temperature);

    /*
     * BMP280 temperature compensation.
     */
    {
        int32_t temp_var1;
        int32_t temp_var2;

        temp_var1 = ((((int32_t)raw_temperature >> 3) -
                      ((int32_t)dig_T1 << 1)) *
                     (int32_t)dig_T2) >> 11;

        temp_var2 = (((((int32_t)raw_temperature >> 4) -
                       (int32_t)dig_T1) *
                      (((int32_t)raw_temperature >> 4) -
                       (int32_t)dig_T1)) >> 12);

        temp_var2 = (temp_var2 * (int32_t)dig_T3) >> 14;

        t_fine = temp_var1 + temp_var2;

        temperature = (t_fine * 5 + 128) >> 8;
    }

    printf("\nCompensated temperature:\n");
    printf("Temperature = %d.%02d C\n",
           temperature / 100,
           temperature % 100);

    printf("t_fine = %d\n", t_fine);

    /*
     * BMP280 pressure compensation.
     */
    var1 = ((int64_t)t_fine) - 128000;

    var2 = var1 * var1 * (int64_t)dig_P6;

    var2 = var2 + ((var1 * (int64_t)dig_P5) << 17);

    var2 = var2 + (((int64_t)dig_P4) << 35);

    var1 = ((var1 * var1 * (int64_t)dig_P3) >> 8) +
           ((var1 * (int64_t)dig_P2) << 12);

    var1 = (((((int64_t)1) << 47) + var1) *
            (int64_t)dig_P1) >> 33;

    if (var1 == 0) {
        fprintf(stderr,
                "Pressure compensation failed: dig_P1 is zero\n");
        close(fd);
    }

    pressure = 1048576 - (int64_t)raw_pressure;

    pressure = (((pressure << 31) - var2) * 3125) / var1;

    var1 = ((int64_t)dig_P9 *
            (pressure >> 13) *
            (pressure >> 13)) >> 25;

    var2 = ((int64_t)dig_P8 * pressure) >> 19;

    pressure = ((pressure + var1 + var2) >> 8) +
               ((int64_t)dig_P7 << 4);

    /*
     * pressure is in Pa multiplied by 256.
     */
    printf("\nCompensated pressure:\n");

    printf("Pressure = %lld Pa\n",
           (long long)(pressure / 256));

    printf("Pressure = %lld.%02lld hPa\n",
           (long long)(pressure / 25600),
           (long long)((pressure % 25600) / 256));

    close(fd);

    return 0;
}
