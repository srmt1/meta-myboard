SUMMARY = "Simple Linux kernel module"
DESCRIPTION = "Example kernel module for Raspberry Pi 1"
LICENSE = "GPL-2.0-only"

LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/GPL-2.0-only;md5=801f80980d171dd6425610833a22dbe6"

SRC_URI = " \
    file://hello.c \
    file://Makefile \
"

S = "${WORKDIR}"

inherit module

RPROVIDES:${PN} += "kernel-module-hello"

do_install() {
    # 1. Create the target destination directory inside the image folder
    install -d ${D}${nonarch_base_libdir}/modules/${KERNEL_VERSION}/extra

    # 2. Manually find and install the module, accounting for potential .xz compression
    if [ -f ${B}/hello.ko.xz ]; then
        install -m 0644 ${B}/hello.ko.xz ${D}${nonarch_base_libdir}/modules/${KERNEL_VERSION}/extra/
    elif [ -f ${B}/hello.ko ]; then
        install -m 0644 ${B}/hello.ko ${D}${nonarch_base_libdir}/modules/${KERNEL_VERSION}/extra/
    fi
}
