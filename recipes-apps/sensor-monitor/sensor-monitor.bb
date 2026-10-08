SUMMARY = "Combined AHT20 and BMP280 sensor monitor"
LICENSE = "CLOSED"

inherit systemd

SRC_URI = " \
    file://sensor-monitor.c \
    file://sensor-monitor.service \
    file://sensor-monitor.conf \
"

S = "${WORKDIR}"

do_compile() {
    ${CC} ${CFLAGS} ${LDFLAGS} \
        ${S}/sensor-monitor.c \
        -o ${B}/sensor-monitor
}

do_install() {
    install -d ${D}${bindir}
    install -d ${D}${sysconfdir}
    install -d ${D}${systemd_system_unitdir}

    install -m 0755 \
        ${B}/sensor-monitor \
        ${D}${bindir}/sensor-monitor

    install -m 0644 \
        ${S}/sensor-monitor.conf \
        ${D}${sysconfdir}/sensor-monitor.conf

    install -m 0644 \
        ${S}/sensor-monitor.service \
        ${D}${systemd_system_unitdir}/sensor-monitor.service
}

SYSTEMD_SERVICE:${PN} = "sensor-monitor.service"
SYSTEMD_AUTO_ENABLE = "enable"

FILES:${PN} += " \
    ${bindir}/sensor-monitor \
    ${sysconfdir}/sensor-monitor.conf \
    ${systemd_system_unitdir}/sensor-monitor.service \
"
