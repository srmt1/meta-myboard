SUMMARY = "Example Device Tree platform driver"
LICENSE = "CLOSED"

inherit module deploy

DEPENDS += "dtc-native"

SRC_URI = "file://my_device.c;subdir=${BP} \
           file://Makefile;subdir=${BP} \
           file://my_device-overlay.dts;subdir=${BP}"

S = "${WORKDIR}/${BP}"

KERNEL_MODULE_AUTOLOAD += "my_device"

do_compile:append() {
    ${STAGING_BINDIR_NATIVE}/dtc \
        -@ \
        -I dts \
        -O dtb \
        -o ${B}/my_device.dtbo \
        ${S}/my_device-overlay.dts
}

do_deploy() {
    install -d ${DEPLOYDIR}

    install -m 0644 \
        ${B}/my_device.dtbo \
        ${DEPLOYDIR}/my_device.dtbo
}

addtask deploy after do_compile before do_package
