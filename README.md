# meta-myboard

Custom Yocto/OpenEmbedded layer for Raspberry Pi embedded Linux development and sensor-driver experimentation.

This project is a practical embedded-Linux learning environment built around a **Raspberry Pi 1 Model B Rev 2**, using **Yocto Project Scarthgap (5.0)**.

The project explores the complete path from direct userspace hardware access through to Linux kernel drivers, Device Tree and the Industrial I/O (IIO) subsystem.

## Hardware

* Raspberry Pi 1 Model B Rev 2
* AHT20 temperature/humidity sensor
* BMP280 temperature/pressure sensor
* I2C bus
* Ethernet
* Serial console
* SSH

The current sensor module contains both an AHT20 and a BMP280 on the same I2C bus:

| Device | I2C address |
| ------ | ----------: |
| AHT20  |      `0x38` |
| BMP280 |      `0x77` |

## Software

The project is based on:

* Yocto Project / Poky Scarthgap 5.0
* Linux kernel 6.6.x
* `meta-raspberrypi`
* `meta-openembedded`
* Custom `meta-myboard` layer
* C userspace applications
* Linux kernel modules
* Device Tree overlays
* I2C
* Industrial I/O (IIO)
* systemd

## Project goals

The main purpose of this project is to understand embedded Linux from the hardware interface upwards.

The development path is:

```text
Userspace I2C
     │
     ▼
C sensor applications
     │
     ▼
Linux I2C subsystem
     │
     ▼
Kernel device drivers
     │
     ▼
Device Tree
     │
     ▼
IIO subsystem
     │
     ▼
Userspace sensor applications
```

The project deliberately starts with direct userspace I2C access so that the sensor protocols and hardware can be understood before moving the functionality into kernel drivers.

## Sensor applications

### AHT20

`aht20-reader` is a C application that communicates directly with the AHT20 using `/dev/i2c-1`.

It demonstrates:

* Opening an I2C device
* Selecting an I2C slave address
* Sending AHT20 commands
* Waiting for a measurement
* Reading sensor data
* Extracting raw temperature and humidity values
* Converting raw values into physical units
* CRC checking

The AHT20 currently runs as a direct userspace I2C device.

### BMP280

`bmp280-reader` is a C application that communicates directly with the BMP280 using `/dev/i2c-1`.

It demonstrates:

* Reading the BMP280 chip ID
* Reading factory calibration coefficients
* Configuring the sensor
* Starting measurements
* Reading raw ADC values
* Applying the Bosch temperature compensation algorithm
* Applying the Bosch pressure compensation algorithm
* Reporting temperature and pressure

This application is primarily a learning implementation. The project also uses the upstream Linux BMP280 driver, which moves this hardware-specific functionality into the kernel.

## BMP280 Linux driver

The Linux kernel already provides an upstream BMP280 driver.

The kernel configuration enables:

```text
CONFIG_BMP280=m
CONFIG_BMP280_I2C=m
CONFIG_IIO=m
```

The BMP280 is described using Device Tree and is bound to the Linux I2C driver.

The resulting architecture is:

```text
Device Tree
     │
     ▼
I2C bus
     │
     ▼
BMP280
     │
     ▼
Linux BMP280 driver
     │
     ▼
IIO subsystem
     │
     ▼
Userspace
```

This provides a useful comparison with the direct `bmp280-reader` implementation.

The userspace implementation contains the BMP280-specific register access and compensation calculations, whereas the kernel-driver approach encapsulates those details in the Linux driver.

## Device Tree

The layer contains a BMP280 Device Tree overlay:

```text
recipes-kernel/bmp280/files/bmp280-overlay.dts
```

The overlay describes the BMP280 on the Raspberry Pi I2C bus and allows the kernel to discover and bind the device to the appropriate driver.

## Custom kernel-driver experiments

The layer also contains several small kernel-driver examples used during the learning process:

```text
recipes-kernel/hello-module/
recipes-kernel/mydriver/
recipes-kernel/my-device/
```

These are experimental examples for learning:

* Kernel module development
* Kernel module loading/unloading
* Device Tree
* Device/driver matching
* `probe()` and `remove()`
* Kernel logging
* Platform devices

The `my-device` example combines a kernel driver with a Device Tree overlay.

## Layer contents

```text
meta-myboard/
├── conf/
│   └── layer.conf
│
├── recipes-apps/
│   ├── aht20-reader/
│   │   ├── aht20-reader.bb
│   │   └── files/
│   │       ├── aht20-reader.c
│   │       ├── sensor_i2c_mutex.c
│   │       └── sensor_i2c_mutex.h
│   │
│   └── bmp280-reader/
│       ├── bmp280-reader.bb
│       └── files/
│           ├── bmp280-reader.c
│           ├── sensor_i2c_mutex.c
│           └── sensor_i2c_mutex.h
│
└── recipes-kernel/
    ├── bmp280/
    │   ├── bmp280-overlay.bb
    │   └── files/
    │       └── bmp280-overlay.dts
    │
    ├── hello-module/
    ├── linux/
    ├── my-device/
    └── mydriver/
```

## Building

The project uses Yocto Project Scarthgap.

The required repositories are:

* Poky
* meta-openembedded
* meta-raspberrypi
* meta-myboard

A repository manifest is provided separately to make it easier to obtain the matching source repositories.

After obtaining the repositories, initialise the Yocto build environment:

```bash
cd poky
source oe-init-build-env build
```

Add the required layers:

```bash
bitbake-layers add-layer ../meta-raspberrypi
bitbake-layers add-layer ../meta-openembedded/meta-oe
bitbake-layers add-layer ../meta-myboard
```

Set:

```text
MACHINE = "raspberrypi"
```

The image can then be built with:

```bash
bitbake core-image-minimal
```

## Development host

The development environment used for this project is:

* Ubuntu 20.04 LTS
* x86-64 development PC

The Raspberry Pi does not require a native C compiler. Applications are cross-compiled by Yocto on the development host and installed into the target image.

## Learning progression

The project is intentionally being developed in stages.

### Stage 1 — Direct userspace I2C

Implement sensor access directly from C:

```text
C application
    ↓
/dev/i2c-1
    ↓
sensor
```

### Stage 2 — Linux kernel driver

Move hardware-specific functionality into a kernel driver:

```text
C application
    ↓
Linux subsystem
    ↓
kernel driver
    ↓
I2C
    ↓
sensor
```

### Stage 3 — Device Tree

Describe hardware using Device Tree rather than hard-coding hardware discovery in applications.

### Stage 4 — IIO

Expose sensor measurements through the Linux Industrial I/O subsystem.

### Stage 5 — systemd

Create services for automatically starting sensor applications.

### Stage 6 — AHT20 kernel driver

The eventual goal is to implement an AHT20 Linux I2C driver and expose the sensor through IIO, providing a direct comparison between a self-written driver and the upstream BMP280 driver.

## Current status

* [x] Yocto Scarthgap build environment
* [x] Raspberry Pi BSP
* [x] I2C enabled
* [x] AHT20 detected at `0x38`
* [x] BMP280 detected at `0x77`
* [x] AHT20 userspace C reader
* [x] BMP280 userspace C reader
* [x] BMP280 kernel module support
* [x] BMP280 Device Tree overlay
* [x] Initial custom kernel-driver examples
* [ ] AHT20 kernel driver
* [ ] AHT20 IIO integration
* [ ] Unified sensor application using IIO
* [ ] systemd sensor-monitor service

## License

This repository contains Yocto metadata and experimental code developed for learning and embedded-Linux development.

Individual components may be subject to their own licenses. See the relevant source files and upstream projects for licensing information.

## Reproducing the source tree

The repository contains a Yocto Project Scarthgap manifest:

```text
manifest/scarthgap.xml
```

The manifest pins the project to the exact source revisions used by the development environment.

The source tree consists of:

```text
poky/
meta-raspberrypi/
meta-myboard/
```

The Poky repository also contains the `meta-openembedded/meta-oe` layer used by this project.

The manifest uses fixed Git commits rather than floating branches so that the source tree can be reproduced reliably.

### Using the manifest

Install Google's `repo` tool if it is not already available, then initialise the project:

```bash
repo init \
    -u https://github.com/srmt1/meta-myboard.git \
    -b main \
    -m manifest/scarthgap.xml
```

Then synchronise the repositories:

```bash
repo sync
```

The resulting directory structure is:

```text
project/
├── poky/
├── meta-raspberrypi/
└── meta-myboard/
```

The Yocto build environment can then be initialised with:

```bash
cd poky
source oe-init-build-env build
```

The required layers can be added with:

```bash
bitbake-layers add-layer ../meta-raspberrypi
bitbake-layers add-layer ../meta-myboard
bitbake-layers add-layer ../poky/meta-openembedded/meta-oe
```

The target machine is:

```text
MACHINE = "raspberrypi"
```

The example image can then be built with:

```bash
bitbake core-image-minimal
```


