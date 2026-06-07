#include "display_status.hpp"

#include <cstdio>

#include "protocol.hpp"

namespace {

const char* mode_text(eps_protocol::Mode mode) {
    switch (mode) {
        case eps_protocol::Mode::Boot:
            return "BOOT";
        case eps_protocol::Mode::Nominal:
            return "NOMINAL";
        case eps_protocol::Mode::Safe:
            return "SAFE";
        default:
            return "UNKNOWN";
    }
}

void format_current(char* out, size_t len, int16_t current_mA_x10) {
    const bool negative = current_mA_x10 < 0;
    int value = negative ? -static_cast<int>(current_mA_x10) : current_mA_x10;
    std::snprintf(out, len, "%s%d.%dMA", negative ? "-" : "", value / 10, value % 10);
}

}  // namespace

void update_status_display(
    Ssd1306Display& display,
    const eps_app::EpsNode& node,
    const eps_app::PowerSample& power_sample,
    uint32_t uptime_ms) {
    if (!display.available()) {
        return;
    }

    char line[22] = {};
    char current[12] = {};

    display.clear();

    std::snprintf(line, sizeof(line), "EPS %s", mode_text(node.mode()));
    display.set_line(0, line);

    std::snprintf(
        line,
        sizeof(line),
        "LOAD %02X F%04X",
        node.effective_load_mask(),
        node.fault_flags());
    display.set_line(1, line);

    if ((power_sample.sensor_status & eps_app::kPowerSensorPresent) != 0) {
        format_current(current, sizeof(current), power_sample.current_mA_x10);
        std::snprintf(line, sizeof(line), "I %s P %uMW", current, power_sample.power_mW);
        display.set_line(2, line);

        std::snprintf(
            line,
            sizeof(line),
            "VBUS %uMV T%lus",
            power_sample.bus_mV,
            static_cast<unsigned long>(uptime_ms / 1000u));
        display.set_line(3, line);
    } else {
        std::snprintf(line, sizeof(line), "PWR SENSOR ERR");
        display.set_line(2, line);

        std::snprintf(
            line,
            sizeof(line),
            "STAT %02X T%lus",
            power_sample.sensor_status,
            static_cast<unsigned long>(uptime_ms / 1000u));
        display.set_line(3, line);
    }

    display.flush();
}
