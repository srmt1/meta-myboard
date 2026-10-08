SUMMARY = "AHT20 I2C kernel driver"
LICENSE = "GPL-2.0-only"
LIC_FILES_CHKSUM = "file://COPYING;md5=b234ee4d69f5fce4486a80fdaf4a4263"

inherit module deploy

DEPENDS += "dtc-native"

SRC_URI = " \
    file://aht20.c \
    file://Makefile \
    file://aht20-overlay.dts \
    file://COPYING \
"

S = "${WORKDIR}"

KERNEL_MODULE_AUTOLOAD += "aht20"

do_compile:append() {
    ${STAGING_BINDIR_NATIVE}/dtc \
        -@ \
        -I dts \
        -O dtb \
        -o ${B}/aht20.dtbo \
        ${S}/aht20-overlay.dts
}

do_install:append() {
    install -d ${D}/boot/overlays

    install -m 0644 \
        ${B}/aht20.dtbo \
        ${D}/boot/overlays/aht20.dtbo
}

do_deploy() {
    install -d ${DEPLOYDIR}

    install -m 0644 \
        ${B}/aht20.dtbo \
        ${DEPLOYDIR}/aht20.dtbo
}

addtask deploy after do_install before do_package

FILES:${PN} += "/boot/overlays/aht20.dtbo"
