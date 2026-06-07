#include "ina219.hpp"

#include <cstdint>

#include "hardware/gpio.h"

namespace {

constexpr uint8_t kRegConfig = 0x00;
constexpr uint8_t kRegShuntVoltage = 0x01;
constexpr uint8_t kRegBusVoltage = 0x02;
constexpr uint8_t kRegPower = 0x03;
constexpr uint8_t kRegCurrent = 0x04;
constexpr uint8_t kRegCalibration = 0x05;

constexpr uint16_t kConfigDefaultContinuous = 0x399F;
constexpr uint16_t kCalibration100uA = 4096;
constexpr uint8_t kBusOverflowMask = 0x01;

uint16_t clamp_u16(uint32_t value) {
    return value > 0xFFFFu ? 0xFFFFu : static_cast<uint16_t>(value);
}

}  // namespace

void Ina219::init(i2c_inst_t* i2c, uint sda_pin, uint scl_pin, uint32_t baud_hz) {
    i2c_ = i2c;
    i2c_init(i2c_, baud_hz);

    gpio_set_function(sda_pin, GPIO_FUNC_I2C);
    gpio_set_function(scl_pin, GPIO_FUNC_I2C);
    gpio_pull_up(sda_pin);
    gpio_pull_up(scl_pin);

    write_register(kRegConfig, kConfigDefaultContinuous);
    write_register(kRegCalibration, kCalibration100uA);
}

bool Ina219::write_register(uint8_t reg, uint16_t value) {
    if (i2c_ == nullptr) {
        return false;
    }
    const uint8_t data[] = {
        reg,
        static_cast<uint8_t>((value >> 8) & 0xFF),
        static_cast<uint8_t>(value & 0xFF),
    };
    return i2c_write_blocking(i2c_, kAddress, data, sizeof(data), false) == static_cast<int>(sizeof(data));
}

bool Ina219::read_register(uint8_t reg, uint16_t& value) {
    if (i2c_ == nullptr) {
        return false;
    }

    const int wrote = i2c_write_blocking(i2c_, kAddress, &reg, 1, true);
    if (wrote != 1) {
        return false;
    }

    uint8_t data[2] = {};
    const int read = i2c_read_blocking(i2c_, kAddress, data, sizeof(data), false);
    if (read != static_cast<int>(sizeof(data))) {
        return false;
    }

    value = (static_cast<uint16_t>(data[0]) << 8) | data[1];
    return true;
}

eps_app::PowerSample Ina219::read_sample() {
    eps_app::PowerSample sample;

    if (!write_register(kRegCalibration, kCalibration100uA)) {
        sample.sensor_status = eps_app::kPowerSensorI2cError;
        return sample;
    }

    uint16_t shunt_raw = 0;
    uint16_t bus_raw = 0;
    uint16_t current_raw = 0;
    uint16_t power_raw = 0;

    if (!read_register(kRegShuntVoltage, shunt_raw) ||
        !read_register(kRegBusVoltage, bus_raw) ||
        !read_register(kRegCurrent, current_raw) ||
        !read_register(kRegPower, power_raw)) {
        sample.sensor_status = eps_app::kPowerSensorI2cError;
        return sample;
    }

    sample.sensor_status = eps_app::kPowerSensorPresent;
    if ((bus_raw & kBusOverflowMask) != 0) {
        sample.sensor_status |= eps_app::kPowerSensorMathOverflow;
    }

    sample.bus_mV = static_cast<uint16_t>(((bus_raw >> 3) & 0x1FFFu) * 4u);
    sample.shunt_uV = static_cast<int32_t>(static_cast<int16_t>(shunt_raw)) * 10;
    sample.current_mA_x10 = static_cast<int16_t>(current_raw);
    sample.power_mW = clamp_u16(static_cast<uint32_t>(power_raw) * 2u);
    return sample;
}
