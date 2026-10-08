SUMMARY = "AHT20 IIO sensor reader"
LICENSE = "CLOSED"

SRC_URI = "file://aht20-reader-iio.c"

S = "${WORKDIR}"

do_compile() {
    ${CC} ${CFLAGS} ${LDFLAGS} \
        ${S}/aht20-reader-iio.c \
        -o ${B}/aht20-reader
}

do_install() {
    install -d ${D}${bindir}

    install -m 0755 \
        ${B}/aht20-reader \
        ${D}${bindir}/aht20-reader
}

FILES:${PN} += "${bindir}/aht20-reader"
