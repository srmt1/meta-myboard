SUMMARY = "BMP280 IIO sensor reader"
LICENSE = "CLOSED"

SRC_URI = "file://bmp280-reader-iio.c"

S = "${WORKDIR}"

do_compile() {
    ${CC} ${CFLAGS} ${LDFLAGS} \
        ${S}/bmp280-reader-iio.c \
        -o ${B}/bmp280-reader
}

do_install() {
    install -d ${D}${bindir}
    install -m 0755 ${B}/bmp280-reader ${D}${bindir}/bmp280-reader
}

FILES:${PN} += "${bindir}/bmp280-reader"
