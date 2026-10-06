SUMMARY = "BMP280 I2C sensor reader"
LICENSE = "CLOSED"

SRC_URI = "file://bmp280-reader.c \
           file://sensor_i2c_mutex.c \
           file://sensor_i2c_mutex.h"

S = "${WORKDIR}"

do_compile() {
    ${CC} ${CFLAGS} ${LDFLAGS} \
        ${S}/bmp280-reader.c \
        ${S}/sensor_i2c_mutex.c \
        -pthread \
        -o ${B}/bmp280-reader
}

do_install() {
    install -d ${D}${bindir}
    install -m 0755 ${B}/bmp280-reader ${D}${bindir}/bmp280-reader
}

FILES:${PN} += "${bindir}/bmp280-reader"
