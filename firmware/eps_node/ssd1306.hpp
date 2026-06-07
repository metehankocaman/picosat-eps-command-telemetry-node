#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "hardware/i2c.h"

class Ssd1306Display {
public:
    bool init(i2c_inst_t* i2c);
    bool available() const;
    uint8_t address() const;

    void clear();
    void set_line(uint8_t line, const char* text);
    bool flush();

private:
    static constexpr uint8_t kWidth = 128;
    static constexpr uint8_t kHeight = 32;
    static constexpr uint8_t kPages = kHeight / 8;
    static constexpr size_t kBufferLen = kWidth * kPages;

    i2c_inst_t* i2c_ = nullptr;
    uint8_t address_ = 0;
    bool available_ = false;
    std::array<uint8_t, kBufferLen> buffer_{};

    bool try_init_address(uint8_t address);
    bool command(uint8_t cmd);
    bool write_controlled(const uint8_t* data, size_t len, uint8_t control);
};
