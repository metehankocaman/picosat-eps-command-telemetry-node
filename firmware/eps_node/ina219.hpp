#pragma once

#include <cstdint>

#include "eps_app.hpp"
#include "hardware/i2c.h"
#include "pico/types.h"

class Ina219 {
public:
    void init(i2c_inst_t* i2c, uint sda_pin, uint scl_pin, uint32_t baud_hz = 100000);
    eps_app::PowerSample read_sample();

private:
    static constexpr uint8_t kAddress = 0x40;

    i2c_inst_t* i2c_ = nullptr;

    bool write_register(uint8_t reg, uint16_t value);
    bool read_register(uint8_t reg, uint16_t& value);
};
