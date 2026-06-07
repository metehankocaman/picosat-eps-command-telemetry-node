#pragma once

#include <cstdint>

#include "eps_app.hpp"
#include "ssd1306.hpp"

void update_status_display(
    Ssd1306Display& display,
    const eps_app::EpsNode& node,
    const eps_app::PowerSample& power_sample,
    uint32_t uptime_ms);
