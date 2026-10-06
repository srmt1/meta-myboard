SUMMARY = "Simple character device driver"
DESCRIPTION = "Example Linux character device driver"
LICENSE = "GPL-2.0-only"
LIC_FILES_CHKSUM = "file://mydriver.c;md5=1aae89ecd168c04a3e52d18524d0b828"

SRC_URI = "file://mydriver.c \
           file://Makefile"

S = "${WORKDIR}"

inherit module

RPROVIDES:${PN} += "kernel-module-mydriver"

