FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

SRC_URI += "file://my-kernel.cfg"

KERNEL_CONFIG_FRAGMENTS += "${WORKDIR}/my-kernel.cfg"

