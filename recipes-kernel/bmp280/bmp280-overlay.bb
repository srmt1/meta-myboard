SUMMARY = "BMP280 Device Tree overlay"
LICENSE = "CLOSED"

inherit deploy

SRC_URI = "file://bmp280-overlay.dts"

S = "${WORKDIR}"

DEPENDS += "dtc-native"

do_compile() {
    dtc -@ -I dts -O dtb \
        -o ${B}/bmp280.dtbo \
        ${S}/bmp280-overlay.dts
}

do_install() {
    install -d ${D}/boot/overlays
    install -m 0644 ${B}/bmp280.dtbo \
        ${D}/boot/overlays/bmp280.dtbo
}

do_deploy() {
    install -m 0644 \
        ${B}/bmp280.dtbo \
        ${DEPLOYDIR}/bmp280.dtbo
}

addtask deploy after do_install before do_build

FILES:${PN} += "/boot/overlays/bmp280.dtbo"
