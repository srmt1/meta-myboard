\
# Raspberry Pi Sensor Platform — Yocto / Linux Driver Project

## 1. Project overview

This project builds a small embedded Linux sensor platform for a **Raspberry Pi 1 Model B Rev 2** using **Yocto Project / Poky Scarthgap**.

The project demonstrates the complete path from hardware to a Linux userspace application:

```text
AHT20 humidity/temperature sensor
        │
        │ I²C
        ▼
   AHT20 kernel driver
        │
        │ IIO
        ▼
 /sys/bus/iio/devices/
        │
        ├──────────────┐
        │              │
        ▼              ▼
 sensor-monitor   aht20-reader-iio
        │
        ▼
     systemd
        │
        ▼
      journal


BMP280 pressure/temperature sensor
        │
        │ I²C
        ▼
 Linux upstream BMP280 driver
        │
        │ IIO
        ▼
 /sys/bus/iio/devices/
        │
        ├──────────────┐
        │              │
        ▼              ▼
 sensor-monitor   bmp280-reader-iio
        │
        ▼
     systemd
```

The project covers:

- Yocto image construction
- Raspberry Pi device-tree overlays
- Linux kernel module development
- I²C device drivers
- Linux Industrial I/O (IIO)
- userspace access through sysfs
- systemd service integration
- C application development
- sensor data conversion
- error handling and retry behaviour
- deployment to real Raspberry Pi hardware

The two sensors are connected to the Raspberry Pi's I²C bus:

| Sensor | I²C address | Linux driver | Userspace interface |
|---|---:|---|---|
| AHT20 | `0x38` | Project AHT20 driver | IIO |
| BMP280 | `0x77` | Linux BMP280 driver | IIO |

The project also contains direct-I²C reader programs. These were developed as reference implementations and are useful for understanding the sensor protocols independently of the Linux kernel drivers.

---

# 2. Hardware

## Target hardware

- Raspberry Pi 1 Model B Rev 2
- AHT20 temperature/humidity sensor
- BMP280 temperature/pressure sensor
- I²C connection
- Ethernet/network connection for SSH
- microSD card containing the Yocto image

The sensor board used during development contains both the AHT20 and BMP280.

The expected I²C addresses are:

```text
AHT20  = 0x38
BMP280 = 0x77
```

After the kernel drivers have claimed the devices, `i2cdetect` is expected to show them as `UU`.

Example:

```text
30: -- -- -- -- -- -- -- -- UU -- -- -- -- -- --
70: -- -- -- -- -- -- -- UU
```

---

# 3. Software architecture

The project deliberately follows the normal Linux embedded-device architecture.

```text
                    Raspberry Pi hardware
                            │
                            │ I²C
                 ┌──────────┴──────────┐
                 │                     │
              AHT20                  BMP280
                 │                     │
                 ▼                     ▼
        Device Tree overlay       Device Tree overlay
                 │                     │
                 ▼                     ▼
        aht20 kernel driver      Linux BMP280 driver
                 │                     │
                 └──────────┬──────────┘
                            │
                           IIO
                            │
                            ▼
                 /sys/bus/iio/devices/
                            │
                ┌───────────┴───────────┐
                │                       │
                ▼                       ▼
        individual readers       sensor-monitor
                                        │
                                        ▼
                                     systemd
                                        │
                                        ▼
                                   journalctl
```

## Why IIO is used

The kernel drivers do the hardware-specific work.

The userspace applications do not need to know:

- I²C transaction details
- AHT20 command sequences
- BMP280 calibration algorithms
- raw ADC register layouts
- kernel driver internals

Instead, they read standard IIO sysfs attributes such as:

```text
/sys/bus/iio/devices/iio:device0/in_temp_input
/sys/bus/iio/devices/iio:device0/in_humidityrelative_input
/sys/bus/iio/devices/iio:device1/in_temp_input
/sys/bus/iio/devices/iio:device1/in_pressure_input
```

The exact `iio:deviceN` number should not be relied upon. The applications therefore search for the IIO device by its `name` attribute.

For example:

```text
iio:device0: aht20
iio:device1: bmp280
```

---

# 4. Device-tree configuration

The sensors are described to Linux using device-tree overlays.

The overlays are installed into:

```text
/boot/overlays/
```

and enabled from:

```text
/boot/config.txt
```

The relevant configuration is:

```text
dtoverlay=aht20
dtoverlay=bmp280
```

This allows the kernel to create the corresponding I²C devices during boot.

For example, the AHT20 overlay describes an AHT20 at address `0x38`, while the BMP280 overlay describes a BMP280 at address `0x77`.

The device-tree mechanism is important because the kernel driver does not simply scan the I²C bus looking for arbitrary hardware. The device-tree description tells Linux which hardware exists and which driver should bind to it.

---

# 5. Repository structure

The custom Yocto layer is:

```text
meta-myboard/
```

The important parts are organised approximately as follows:

```text
meta-myboard/
│
├── conf/
│   └── layer.conf
│
├── recipes-kernel/
│   └── aht20/
│       ├── aht20.bb
│       └── files/
│           ├── aht20.c
│           ├── Makefile
│           ├── aht20-overlay.dts
│           └── COPYING
│
├── recipes-kernel/
│   └── bmp280/
│       ├── bmp280.bb
│       └── files/
│           └── bmp280-overlay.dts
│
├── recipes-apps/
│   ├── aht20-reader/
│   │   ├── aht20-reader.bb
│   │   └── files/
│   │       ├── aht20-reader-i2c.c
│   │       └── ...
│   │
│   ├── bmp280-reader/
│   │   ├── bmp280-reader.bb
│   │   └── files/
│   │       ├── bmp280-reader-i2c.c
│   │       └── ...
│   │
│   ├── aht20-reader-iio/
│   │   ├── aht20-reader-iio.bb
│   │   └── files/
│   │       └── aht20-reader-iio.c
│   │
│   ├── bmp280-reader-iio/
│   │   ├── bmp280-reader-iio.bb
│   │   └── files/
│   │       └── bmp280-reader-iio.c
│   │
│   └── sensor-monitor/
│       ├── sensor-monitor.bb
│       ├── files/
│       │   ├── sensor-monitor.c
│       │   ├── sensor-monitor.service
│       │   └── sensor-monitor.conf
│       └── ...
│
└── README.md
```

The exact repository may contain additional learning/demo recipes; the sensor path described above is the main completed application.

---

# 6. AHT20 kernel driver

## `recipes-kernel/aht20/aht20.bb`

This is the BitBake recipe for the AHT20 kernel driver.

Its responsibilities are:

1. Build `aht20.c` as an out-of-tree kernel module.
2. Compile the device-tree overlay.
3. Install the overlay into `/boot/overlays`.
4. Deploy the overlay into the Yocto image deployment directory.
5. Package the overlay.
6. Configure the module for automatic loading.

Important settings include:

```bitbake
inherit module deploy
```

which provides the Yocto machinery needed to build a kernel module.

```bitbake
KERNEL_MODULE_AUTOLOAD += "aht20"
```

which requests automatic loading of the AHT20 module.

The recipe also uses `dtc-native` to compile:

```text
aht20-overlay.dts
```

into:

```text
aht20.dtbo
```

---

# 7. `aht20.c`

This is the project-written Linux kernel driver for the AHT20.

The driver:

- communicates with the sensor over I²C
- initialises the sensor
- starts measurements
- waits for the measurement to complete
- reads the measurement data
- checks the CRC
- converts raw sensor values
- exposes temperature and humidity through IIO

## `struct aht20_data`

```c
struct aht20_data {
    struct i2c_client *client;
};
```

This stores the I²C client associated with the sensor.

### Input

- `struct i2c_client *client`

### Output

No return value. The structure is stored as the driver's private IIO data.

---

## `aht20_crc8()`

Calculates the AHT20 CRC-8 checksum.

### Inputs

```text
data
len
```

- `data`: pointer to the bytes covered by the CRC
- `len`: number of bytes

### Output

```text
u8
```

Returns the calculated CRC.

The AHT20 uses:

```text
Polynomial: 0x31
Initial CRC: 0xFF
```

---

## `aht20_read_status()`

Reads the AHT20 status byte.

The AHT20 status is obtained with a direct one-byte I²C read.

### Inputs

```text
struct aht20_data *data
u8 *status
```

### Output

Returns:

```text
0       success
negative errno  failure
```

On success, `*status` contains the sensor status byte.

Important status bits include:

```text
bit 7 = measurement busy
bit 3 = calibration enabled
```

---

## `aht20_initialize()`

Initialises the AHT20 if it is not already calibrated.

### Inputs

```text
struct aht20_data *data
```

### Output

Returns:

```text
0       success
negative errno  failure
```

The function:

1. reads the status byte
2. checks the calibration bit
3. sends the AHT20 initialisation command if necessary
4. waits for initialisation
5. reads status again
6. verifies calibration completed

---

## `aht20_measure()`

Performs one complete AHT20 measurement.

### Inputs

```text
struct aht20_data *data
s32 *temperature
s32 *humidity
```

### Output

Returns:

```text
0       success
negative errno  failure
```

On success:

```text
*temperature = temperature in thousandths of a degree Celsius
*humidity    = relative humidity in thousandths of a percent
```

For example:

```text
20720
```

means:

```text
20.720 °C
```

and:

```text
84480
```

means:

```text
84.480 %
```

The function:

1. sends the measurement trigger
2. polls the status byte
3. waits until the BUSY bit clears
4. reads the seven-byte measurement
5. checks the CRC
6. extracts the raw humidity
7. extracts the raw temperature
8. converts the values using fixed-point arithmetic

The driver intentionally avoids 64-bit division because the Raspberry Pi 1 ARM kernel environment does not provide the required 64-bit division helper for this module.

---

## `aht20_read_raw()`

This is the IIO callback used when userspace requests a processed sensor value.

### Inputs

```text
struct iio_dev *indio_dev
struct iio_chan_spec const *chan
int *val
int *val2
long mask
```

### Output

Returns an IIO status such as:

```text
IIO_VAL_INT
```

or a negative errno.

For temperature and humidity, the function places the processed value into:

```text
*val
```

The IIO interface therefore presents the sensor values as integer values in milli-units.

---

## `aht20_probe()`

Called by the Linux I²C subsystem when the AHT20 device is found.

### Inputs

```text
struct i2c_client *client
```

### Output

Returns:

```text
0       successful probe
negative errno  failure
```

The function:

1. allocates the IIO device
2. creates the driver's private data
3. associates the I²C client with the driver
4. sets the IIO device name
5. defines the temperature and humidity channels
6. initialises the AHT20
7. registers the IIO device

This is the point at which the hardware becomes a Linux IIO device.

---

## Device-tree matching

The driver contains:

```text
compatible = "aosong,aht20"
```

The AHT20 device-tree overlay uses the same compatible string.

This allows Linux to match:

```text
device-tree node
       ↓
aht20 kernel driver
```

---

# 8. AHT20 device-tree overlay

## `aht20-overlay.dts`

This describes the AHT20 hardware to Linux.

The important information is:

```text
I²C bus: I²C1
address: 0x38
compatible: aosong,aht20
```

The compiled result is:

```text
aht20.dtbo
```

which is installed under:

```text
/boot/overlays/aht20.dtbo
```

---

# 9. AHT20 Makefile

## `Makefile`

The Makefile provides the targets expected by the Yocto `module` class.

Important targets are:

```text
all
modules
modules_install
clean
```

The module itself is:

```text
aht20.o
```

and is built against the Yocto kernel source/build tree.

The `modules_install` target is required because the Yocto `inherit module` infrastructure uses it during installation.

---

# 10. `COPYING`

## `COPYING`

Contains the GPL-2 license text used by the AHT20 kernel driver.

The BitBake recipe uses it for:

```text
LIC_FILES_CHKSUM
```

so Yocto can verify the declared license.

---

# 11. BMP280 device-tree overlay

## `recipes-kernel/bmp280/files/bmp280-overlay.dts`

The BMP280 overlay describes a BMP280 connected to I²C1 at:

```text
0x77
```

It uses:

```text
compatible = "bosch,bmp280"
```

This matches the Linux kernel's BMP280 driver.

The overlay is compiled to:

```text
bmp280.dtbo
```

and installed to:

```text
/boot/overlays/bmp280.dtbo
```

The boot configuration contains:

```text
dtoverlay=bmp280
```

Unlike the AHT20, the BMP280 driver itself is supplied by the Linux kernel.

---

# 12. Linux BMP280 driver

The project does not implement the BMP280 kernel driver.

It uses the upstream Linux driver:

```text
bmp280
bmp280_i2c
```

The relevant kernel configuration is:

```text
CONFIG_BMP280=m
CONFIG_BMP280_I2C=m
```

The modules are installed in the target image.

The driver performs:

- I²C communication
- chip identification
- calibration-data handling
- raw ADC acquisition
- Bosch compensation calculations
- IIO registration

The resulting device appears as an IIO device named:

```text
bmp280
```

---

# 13. Direct AHT20 userspace reader

## `aht20-reader-i2c.c`

This is a userspace reference implementation that communicates directly with:

```text
/dev/i2c-1
```

It does not use the AHT20 kernel driver.

Its purpose is to demonstrate the AHT20 protocol independently.

It performs:

1. I²C device selection
2. sensor initialisation if required
3. measurement triggering
4. measurement delay
5. reading the seven-byte response
6. CRC checking
7. raw-value extraction
8. temperature conversion
9. humidity conversion

This program is useful for understanding the sensor before moving the hardware handling into the kernel.

---

# 14. Direct BMP280 userspace reader

## `bmp280-reader-i2c.c`

This is the direct-I²C BMP280 reference implementation.

It communicates with:

```text
/dev/i2c-1
```

rather than using the Linux BMP280 kernel driver.

It demonstrates:

- reading the BMP280 chip ID
- reading calibration registers
- configuring the sensor
- reading raw ADC values
- performing Bosch compensation
- converting temperature
- converting pressure

Example raw-value extraction:

```text
raw_pressure
raw_temperature
```

The program then calculates:

```text
Temperature = °C
Pressure = Pa
Pressure = hPa
```

This application is mainly a learning/reference implementation. The normal project architecture uses the Linux BMP280 driver instead.

---

# 15. AHT20 IIO userspace reader

## `aht20-reader-iio.c`

This program demonstrates the preferred userspace interface.

It does not communicate with I²C directly.

Instead it:

1. searches `/sys/bus/iio/devices`
2. finds the IIO device whose `name` is `aht20`
3. reads `in_temp_input`
4. reads `in_humidityrelative_input`
5. converts milli-units into human-readable units
6. prints the result

## `find_iio_device()`

### Inputs

```text
name
path
path_size
```

- `name`: required IIO device name
- `path`: output buffer
- `path_size`: output buffer size

### Output

```text
0       device found
-1      device not found
```

On success, `path` contains the IIO device directory.

---

## `read_value()`

Reads one numeric sysfs value.

### Inputs

```text
path
value
```

- `path`: sysfs file to read
- `value`: output floating-point value

### Output

```text
0       success
-1      failure
```

---

## `main()`

Finds the AHT20 IIO device and reads:

```text
in_temp_input
in_humidityrelative_input
```

It then converts:

```text
millidegrees Celsius → degrees Celsius
millipercent → percent
```

Example:

```text
AHT20 via IIO
Temperature: 20.300 °C
Humidity:    84.590 %
```

---

# 16. BMP280 IIO userspace reader

## `bmp280-reader-iio.c`

This program is the BMP280 equivalent of the AHT20 IIO reader.

It searches for:

```text
name = bmp280
```

and reads:

```text
in_temp_input
in_pressure_input
```

The pressure value supplied by the IIO interface is converted into hPa.

## `find_iio_device()`

### Inputs

```text
name
path
path_size
```

### Output

```text
0       device found
-1      device not found
```

The function searches the IIO device directories rather than assuming that BMP280 is always `iio:device1`.

---

## `read_value()`

### Inputs

```text
path
value
```

### Output

```text
0       success
-1      failure
```

Reads the numeric value from the specified sysfs file.

---

## `main()`

Reads:

```text
in_temp_input
in_pressure_input
```

and prints:

```text
BMP280 via IIO
Temperature: 21.100 °C
Pressure:    1010.95 hPa
```

---

# 17. Combined sensor monitor

## `sensor-monitor.c`

This is the main userspace application.

It combines both sensors into one application and provides a long-running monitoring service.

Its architecture is:

```text
              IIO
               │
       ┌───────┴───────┐
       │               │
     AHT20           BMP280
       │               │
       └───────┬───────┘
               │
        sensor-monitor
               │
               ▼
        timestamp + readings
```

The application does not directly access I²C.

It uses the IIO sysfs interface.

The current design includes:

- automatic discovery of both IIO devices
- waiting for the devices to appear during boot
- configurable sampling interval
- timestamped output
- transient read retries
- rediscovery after failures
- clean SIGINT/SIGTERM handling

---

## `find_iio_device()`

Finds an IIO device by its `name`.

### Inputs

```text
name
path
path_size
```

### Output

```text
0       success
-1      failure
```

The output path identifies the IIO device directory.

---

## `read_value()`

Reads a numeric value from a sysfs attribute.

### Inputs

```text
path
value
```

### Output

```text
0       success
-1      failure
```

---

## `print_timestamp()`

Prints the current local date and time.

Example:

```text
2026-10-06 14:32:15
```

It uses the system local time.

### Inputs

None.

### Output

Writes the timestamp to standard output.

---

## Configuration handling

The application reads:

```text
/etc/sensor-monitor.conf
```

The current configuration is:

```text
INTERVAL_SECONDS=5
```

The interval is constrained to a sensible range:

```text
minimum = 1 second
maximum = 3600 seconds
```

The default is:

```text
5 seconds
```

if the configuration file does not provide a valid value.

---

## Signal handling

The application handles:

```text
SIGINT
SIGTERM
```

This allows the program to exit cleanly when:

```text
systemctl stop sensor-monitor
```

is used.

---

## Retry and rediscovery behaviour

The monitor does not assume that a sensor read will always succeed.

If a read fails temporarily, it retries.

If repeated failures occur, the application attempts to rediscover the IIO devices.

This is useful for a real embedded system because device availability can change independently of the userspace process.

---

# 18. `sensor-monitor.conf`

## `/etc/sensor-monitor.conf`

Current contents:

```text
INTERVAL_SECONDS=5
```

This controls how frequently measurements are taken.

For example:

```text
INTERVAL_SECONDS=10
```

requests a ten-second measurement interval.

---

# 19. `sensor-monitor.service`

The systemd unit runs the monitor automatically.

The service is configured approximately as:

```ini
[Unit]
Description=AHT20 and BMP280 sensor monitor
After=local-fs.target
Wants=local-fs.target

[Service]
Type=simple
ExecStart=/usr/bin/sensor-monitor
Restart=on-failure
RestartSec=5
TimeoutStopSec=10

[Install]
WantedBy=multi-user.target
```

The application itself waits for the IIO devices, so the service does not depend on the global `systemd-udev-settle.service`.

This keeps the boot dependency simple.

---

# 20. `sensor-monitor.bb`

The BitBake recipe:

- compiles `sensor-monitor.c`
- installs the executable
- installs the configuration file
- installs the systemd unit
- enables the service
- packages all three files

The important Yocto mechanism is:

```bitbake
inherit systemd
```

and:

```bitbake
SYSTEMD_SERVICE:${PN} = "sensor-monitor.service"
SYSTEMD_AUTO_ENABLE = "enable"
```

Therefore the service is intended to start automatically at boot.

---

# 21. Yocto image configuration

The image configuration enables the required Raspberry Pi and Linux functionality.

Important configuration includes:

```text
ENABLE_I2C = "1"
```

and the IIO/sensor applications are included in the image.

The boot configuration enables:

```text
dtoverlay=aht20
dtoverlay=bmp280
```

The corresponding overlays are copied to:

```text
/boot/overlays/
```

The system uses systemd as PID 1.

Relevant configuration is:

```text
DISTRO_FEATURES:append = " zeroconf systemd usrmerge"
VIRTUAL-RUNTIME_init_manager = "systemd"
```

---

# 22. Boot sequence

At boot, the important sequence is:

```text
1. Raspberry Pi firmware starts
          │
          ▼
2. config.txt loads AHT20/BMP280 overlays
          │
          ▼
3. Linux kernel starts
          │
          ▼
4. I²C bus becomes available
          │
          ▼
5. AHT20 device is created at 0x38
6. BMP280 device is created at 0x77
          │
          ▼
7. Kernel drivers bind
          │
          ├── AHT20 project driver
          └── BMP280 Linux driver
          │
          ▼
8. IIO devices are registered
          │
          ├── aht20
          └── bmp280
          │
          ▼
9. systemd starts sensor-monitor
          │
          ▼
10. sensor-monitor finds both IIO devices
          │
          ▼
11. measurements are read periodically
```

---

# 23. Expected target filesystem

Important files/directories on the Raspberry Pi include:

```text
/boot/config.txt
/boot/overlays/aht20.dtbo
/boot/overlays/bmp280.dtbo

/usr/bin/aht20-reader
/usr/bin/bmp280-reader
/usr/bin/aht20-reader-iio
/usr/bin/bmp280-reader-iio
/usr/bin/sensor-monitor

/etc/sensor-monitor.conf

/etc/systemd/system/sensor-monitor.service

/sys/bus/iio/devices/
```

The kernel modules include the AHT20 module and the required BMP280/IIO modules.

---

# 24. Checking the hardware on the target

After booting the Raspberry Pi, SSH into it:

```bash
ssh root@<PI_IP_ADDRESS>
```

Check I²C:

```bash
i2cdetect -y 1
```

Expected devices:

```text
0x38  AHT20
0x77  BMP280
```

Once the kernel drivers have claimed them, they normally appear as:

```text
UU
```

rather than the hexadecimal address.

---

# 25. Check the IIO devices

Run:

```bash
ls -l /sys/bus/iio/devices/
```

Then:

```bash
cat /sys/bus/iio/devices/iio:device0/name
cat /sys/bus/iio/devices/iio:device1/name
```

The device numbers are not guaranteed, but the expected names are:

```text
aht20
bmp280
```

A typical result is:

```text
iio:device0: aht20
iio:device1: bmp280
```

---

# 26. Read AHT20 through IIO

Run:

```bash
aht20-reader-iio
```

Expected output is similar to:

```text
AHT20 via IIO
Temperature: 20.300 °C
Humidity:    84.590 %
```

The exact values depend on the room temperature and humidity.

---

# 27. Read BMP280 through IIO

Run:

```bash
bmp280-reader-iio
```

Expected output is similar to:

```text
BMP280 via IIO
Temperature: 21.100 °C
Pressure:    1010.95 hPa
```

Again, the exact values depend on the environment and atmospheric pressure.

---

# 28. Run the combined monitor manually

Run:

```bash
sensor-monitor
```

The application should print measurements periodically.

A typical output pattern is:

```text
2026-10-06 14:32:15
AHT20: Temperature: 20.300 °C, Humidity: 84.590 %
BMP280: Temperature: 21.100 °C, Pressure: 1010.95 hPa

2026-10-06 14:32:20
AHT20: Temperature: 20.310 °C, Humidity: 84.520 %
BMP280: Temperature: 21.110 °C, Pressure: 1010.93 hPa
```

The actual formatting and sensor values depend on the current version of `sensor-monitor.c`.

Press:

```text
Ctrl+C
```

to stop the application cleanly.

---

# 29. Check the systemd service

Check its status:

```bash
systemctl status sensor-monitor
```

Check whether it is enabled:

```bash
systemctl is-enabled sensor-monitor
```

Expected:

```text
enabled
```

Check whether it is running:

```bash
systemctl is-active sensor-monitor
```

Expected:

```text
active
```

---

# 30. View sensor-monitor logs

The application is intended to run under systemd.

View its logs with:

```bash
journalctl -u sensor-monitor
```

Follow the output live:

```bash
journalctl -u sensor-monitor -f
```

View recent entries:

```bash
journalctl -u sensor-monitor -n 20
```

This provides the normal embedded-Linux workflow:

```text
sensor-monitor
      │
      ▼
  stdout/stderr
      │
      ▼
   systemd
      │
      ▼
   journald
      │
      ▼
 journalctl
```

---

# 31. Starting, stopping and restarting

Start:

```bash
systemctl start sensor-monitor
```

Stop:

```bash
systemctl stop sensor-monitor
```

Restart:

```bash
systemctl restart sensor-monitor
```

The service is configured to restart after an unexpected failure:

```text
Restart=on-failure
```

---

# 32. Relationship between the test applications and sensor-monitor

There are two levels of userspace software in the project.

## Individual readers

```text
aht20-reader-i2c
bmp280-reader-i2c
```

These communicate directly with I²C and are primarily educational/reference programs.

Then:

```text
aht20-reader-iio
bmp280-reader-iio
```

These demonstrate the preferred Linux architecture:

```text
userspace
   ↓
IIO
   ↓
kernel driver
   ↓
I²C
   ↓
sensor
```

Finally:

```text
sensor-monitor
```

combines both IIO devices into a continuously running application.

---

# 33. What this project demonstrates

This project demonstrates several important embedded Linux concepts.

## Hardware description

Device Tree tells Linux what hardware is connected.

```text
Device Tree
    ↓
I²C device
```

## Kernel driver binding

Linux matches the device-tree `compatible` string against a driver's device table.

```text
"aosong,aht20"
       ↓
AHT20 driver
```

## Kernel-to-userspace interface

The AHT20 and BMP280 drivers expose standard IIO interfaces.

```text
kernel driver
     ↓
    IIO
     ↓
   sysfs
```

## Userspace applications

Userspace programs consume the standard interface rather than implementing hardware-specific I²C protocols.

```text
C application
     ↓
/sys/bus/iio/devices
```

## Service management

systemd runs the monitor automatically.

```text
systemd
   ↓
sensor-monitor
```

## Logging

systemd captures the application's output.

```text
sensor-monitor
      ↓
 journald
      ↓
 journalctl
```

## Yocto integration

All of the above is built into a reproducible embedded Linux image through BitBake recipes.

---

# 34. Development workflow

The normal development cycle is:

```text
Edit source
    ↓
bitbake <recipe>
    ↓
bitbake core-image-minimal
    ↓
flash SD card
    ↓
boot Raspberry Pi
    ↓
SSH
    ↓
test
    ↓
inspect dmesg / journalctl
    ↓
modify source
```

For normal incremental development, a full clean build is not required.

For example:

```bash
cd ~/yocto/poky/build
bitbake sensor-monitor
bitbake core-image-minimal
```

BitBake's shared-state/cache system allows unchanged components to be reused.

---

# 35. Useful kernel/debug commands

Check kernel messages:

```bash
dmesg | tail -50
```

Search for AHT20:

```bash
dmesg | grep -i aht20
```

Check loaded modules:

```bash
lsmod
```

Check I²C devices:

```bash
ls -l /sys/bus/i2c/devices/
```

Check IIO devices:

```bash
ls -l /sys/bus/iio/devices/
```

Check the AHT20 IIO device:

```bash
cat /sys/bus/iio/devices/iio:device0/name
```

Check the BMP280 IIO device:

```bash
cat /sys/bus/iio/devices/iio:device1/name
```

Do not rely on `device0` and `device1` permanently; use the `name` attribute to identify the devices.

---

# 36. Summary of the completed data path

The complete AHT20 path is:

```text
AHT20 hardware
     │
     │ I²C address 0x38
     ▼
Device Tree
     │
     ▼
aht20.c
     │
     │ measurement + CRC + conversion
     ▼
Linux IIO
     │
     ▼
/sys/bus/iio/devices/
     │
     ▼
sensor-monitor
     │
     ▼
systemd
     │
     ▼
journalctl
```

The BMP280 path is:

```text
BMP280 hardware
     │
     │ I²C address 0x77
     ▼
Device Tree
     │
     ▼
Linux BMP280 driver
     │
     │ calibration + compensation
     ▼
Linux IIO
     │
     ▼
/sys/bus/iio/devices/
     │
     ▼
sensor-monitor
     │
     ▼
systemd
     │
     ▼
journalctl
```

This separation is intentional: hardware-specific work belongs in the kernel driver, while the application consumes the standard Linux IIO interface.

---

# 37. Project status

The following major stages are complete:

- [x] Raspberry Pi Yocto image
- [x] I²C support
- [x] AHT20 direct-I²C userspace reader
- [x] BMP280 direct-I²C userspace reader
- [x] BMP280 Device Tree overlay
- [x] Linux BMP280 driver
- [x] BMP280 IIO interface
- [x] BMP280 IIO userspace reader
- [x] AHT20 Linux kernel driver
- [x] AHT20 Device Tree overlay
- [x] AHT20 IIO interface
- [x] AHT20 IIO userspace reader
- [x] Combined sensor-monitor application
- [x] Configurable sensor-monitor interval
- [x] systemd integration
- [x] Automatic sensor-monitor startup
- [x] journald/systemd logging

The project therefore provides a complete working example of developing and integrating a custom Linux sensor driver into a Yocto-built embedded Linux system.

---

# 38. Build environment

The project was developed using:

```text
Target:
    Raspberry Pi 1 Model B Rev 2

Yocto:
    Scarthgap / Yocto 5

Machine:
    raspberrypi

Custom layer:
    meta-myboard

Build image:
    core-image-minimal
```

The repository also records the project layer state and is intended to provide a version-controlled record of the custom Yocto work.

# 39. `conf/local.conf` project configuration

The following `local.conf` settings are part of the project setup. They control the build parallelism, target machine, image features, networking, systemd, I²C, kernel modules, device-tree overlays and applications included in the image.

## Build parallelism

```bitbake
BB_NUMBER_THREADS = "4"
PARALLEL_MAKE = "-j4"
```

These allow BitBake to execute up to four tasks in parallel and pass `-j4` to Make-based builds.

In practical terms:

```text
BitBake task parallelism  →  4
Make job parallelism      →  4
```

The exact performance benefit depends on the host CPU, RAM and the particular build stage.

---

## Target machine

```bitbake
MACHINE = "raspberrypi"
```

This selects the Raspberry Pi machine configuration supplied by `meta-raspberrypi`.

It causes Yocto to build an image appropriate for the Raspberry Pi target used by this project.

---

## Development shortcuts

```bitbake
# Enable development shortcuts (empty root password)
EXTRA_IMAGE_FEATURES += "debug-tweaks"
```

`debug-tweaks` enables development-oriented image behaviour.

In this project it is useful during development because it permits convenient root access without requiring a configured root password.

This should be regarded as a **development configuration**, rather than a production security configuration.

---

## SSH server

```bitbake
IMAGE_FEATURES += "ssh-server-dropbear"
```

This adds the Dropbear SSH server to the target image.

The result is that the Raspberry Pi can be accessed remotely with:

```bash
ssh root@<PI_IP_ADDRESS>
```

Dropbear is particularly suitable for a small embedded Linux image because it provides SSH functionality with relatively low resource requirements.

---

## DHCP/networking

```bitbake
IMAGE_INSTALL:append = " dhcpcd init-ifupdown"
```

These packages provide the networking components used by the target.

`dhcpcd` provides DHCP client functionality, allowing the Raspberry Pi to obtain an IP address automatically from a DHCP server.

`init-ifupdown` provides the traditional interface configuration support.

This makes it possible to boot the Pi, obtain an address on Ethernet and then connect using SSH.

---

## Distribution features

```bitbake
DISTRO_FEATURES:append = " zeroconf systemd usrmerge"
```

Three important features are enabled.

### `zeroconf`

Enables zero-configuration networking support in the distribution feature set.

### `systemd`

Selects systemd-related functionality in the image.

This is required by the project's `sensor-monitor.service`.

### `usrmerge`

Enables the `/usr` merge layout required by the systemd configuration used by this Scarthgap setup.

---

## systemd as PID 1

```bitbake
VIRTUAL-RUNTIME_init_manager = "systemd"
```

This selects systemd as the target's init manager.

Consequently, the normal process hierarchy is:

```text
PID 1
 │
 ▼
systemd
 │
 ├── networking
 ├── kernel/device management
 └── sensor-monitor.service
```

The project therefore uses systemd rather than the alternative init system.

---

## BMP280 Device Tree overlay

```bitbake
RPI_EXTRA_CONFIG:append = "\ndtoverlay=bmp280\n"
```

This adds the BMP280 overlay to the Raspberry Pi firmware configuration.

At boot, the firmware loads:

```text
/boot/overlays/bmp280.dtbo
```

The overlay describes the BMP280 on I²C1 at address:

```text
0x77
```

The resulting Linux device is then matched with the upstream BMP280 driver.

---

## Deploying the BMP280 overlay

```bitbake
IMAGE_BOOT_FILES:append = " bmp280.dtbo;overlays/bmp280.dtbo"
```

This tells the image construction process to copy:

```text
bmp280.dtbo
```

from the Yocto image deployment area into:

```text
/boot/overlays/bmp280.dtbo
```

on the target boot partition.

The two names have different roles:

```text
bmp280.dtbo                 source/deployment filename
overlays/bmp280.dtbo        destination on the boot partition
```

---

## Automatically loading `i2c-dev`

```bitbake
KERNEL_MODULE_AUTOLOAD += "i2c-dev"
```

This requests automatic loading of the Linux `i2c-dev` module.

It provides the userspace I²C character-device interface, such as:

```text
/dev/i2c-1
```

This is particularly useful for the direct-I²C reference applications and for tools such as `i2cdetect`.

The normal sensor-monitor architecture uses the kernel drivers and IIO rather than directly accessing `/dev/i2c-1`.

---

## Enable Raspberry Pi I²C

```bitbake
ENABLE_I2C = "1"
```

This enables I²C support in the Raspberry Pi machine configuration.

The project uses I²C1 for the sensors:

```text
I²C1
 │
 ├── 0x38  AHT20
 └── 0x77  BMP280
```

---

## Install I²C tools

```bitbake
IMAGE_INSTALL:append = " i2c-tools"
```

This installs the Linux I²C userspace tools.

The most useful command during development is:

```bash
i2cdetect -y 1
```

which can be used to inspect devices on I²C bus 1.

Before drivers claim the devices, the addresses can appear as:

```text
38
77
```

After the kernel drivers have claimed them, they normally appear as:

```text
UU
```

---

## Install the BMP280 kernel modules

```bitbake
IMAGE_INSTALL:append = " kernel-module-bmp280 kernel-module-bmp280-i2c"
```

These packages install the Linux BMP280 driver and its I²C transport component into the target image.

The relevant kernel configuration is:

```text
CONFIG_BMP280=m
CONFIG_BMP280_I2C=m
```

The resulting modules allow the upstream Linux driver to communicate with the BMP280 over I²C.

---

## Install the project applications

```bitbake
IMAGE_INSTALL:append = " aht20-reader bmp280-reader sensor-monitor"
```

This installs the project's userspace applications:

```text
aht20-reader
bmp280-reader
sensor-monitor
```

The first two are the direct-I²C reference applications.

`sensor-monitor` is the main long-running application used with the IIO interfaces and systemd.

The IIO reader applications are also part of the project where included by their corresponding image configuration/recipes.

---

## AHT20 Device Tree overlay

```bitbake
RPI_EXTRA_CONFIG:append = "\ndtoverlay=aht20\n"
```

This tells the Raspberry Pi firmware to load the AHT20 overlay at boot.

The overlay describes:

```text
I²C bus:  I²C1
address: 0x38
compatible: aosong,aht20
```

The compatible string allows the Linux I²C subsystem to bind the project AHT20 driver.

---

## Deploying the AHT20 overlay

```bitbake
IMAGE_BOOT_FILES:append = " aht20.dtbo;overlays/aht20.dtbo"
```

This copies:

```text
aht20.dtbo
```

to:

```text
/boot/overlays/aht20.dtbo
```

in the target boot partition.

The resulting boot configuration is therefore:

```text
/boot/config.txt
    │
    ├── dtoverlay=bmp280
    └── dtoverlay=aht20
```

---

## Install the AHT20 kernel module

```bitbake
IMAGE_INSTALL:append = " aht20"
```

This installs the project-built AHT20 kernel module into the target image.

At boot, the module is also configured for automatic loading by the AHT20 BitBake recipe:

```bitbake
KERNEL_MODULE_AUTOLOAD += "aht20"
```

The complete AHT20 startup path is therefore:

```text
aht20.dtbo
    ↓
I²C device at 0x38
    ↓
aht20 kernel module
    ↓
IIO device named "aht20"
    ↓
sensor-monitor
```

---

## Default distribution

```bitbake
DISTRO ?= "poky"
```

This selects Poky as the default Yocto distribution if `DISTRO` has not already been set elsewhere.

Poky supplies the base Yocto distribution configuration used by the project.

---

## Complete `local.conf` configuration

For reference, the project configuration described above is:

```bitbake
BB_NUMBER_THREADS = "4"
PARALLEL_MAKE = "-j4"

MACHINE = "raspberrypi"

# Enable development shortcuts (empty root password)
EXTRA_IMAGE_FEATURES += "debug-tweaks"

# Install Dropbear SSH server and auto-networking tools
IMAGE_FEATURES += "ssh-server-dropbear"
IMAGE_INSTALL:append = " dhcpcd init-ifupdown"

DISTRO_FEATURES:append = " zeroconf systemd usrmerge"
VIRTUAL-RUNTIME_init_manager = "systemd"

RPI_EXTRA_CONFIG:append = "\ndtoverlay=bmp280\n"
IMAGE_BOOT_FILES:append = " bmp280.dtbo;overlays/bmp280.dtbo"

KERNEL_MODULE_AUTOLOAD += "i2c-dev"

ENABLE_I2C = "1"

IMAGE_INSTALL:append = " i2c-tools"

IMAGE_INSTALL:append = " kernel-module-bmp280 kernel-module-bmp280-i2c"

IMAGE_INSTALL:append = " aht20-reader bmp280-reader sensor-monitor"

RPI_EXTRA_CONFIG:append = "\ndtoverlay=aht20\n"
IMAGE_BOOT_FILES:append = " aht20.dtbo;overlays/aht20.dtbo"

IMAGE_INSTALL:append = " aht20"

DISTRO ?= "poky"
```

This configuration, together with the custom `meta-myboard` recipes, produces the development image used by the project.

