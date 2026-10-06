SUMMARY = "AHT20 I2C sensor reader"
LICENSE = "CLOSED"

SRC_URI = "file://aht20-reader.c \
           file://sensor_i2c_mutex.c \
           file://sensor_i2c_mutex.h"

S = "${WORKDIR}"

do_compile() {
    ${CC} ${CFLAGS} ${LDFLAGS} \
      ${S}/aht20-reader.c \
      ${S}/sensor_i2c_mutex.c \
      -pthread \
      -o ${B}/aht20-reader
}

do_install() {
    install -d ${D}${bindir}
    install -m 0755 ${B}/aht20-reader ${D}${bindir}/aht20-reader
}

FILES:${PN} += "${bindir}/aht20-reader"
