#include "ssd1306.hpp"

#include <algorithm>
#include <array>
#include <cstring>

namespace {

using Glyph = std::array<uint8_t, 5>;

Glyph glyph_for(char c) {
    if (c >= 'a' && c <= 'z') {
        c = static_cast<char>(c - 'a' + 'A');
    }

    switch (c) {
        case ' ': return {0x00, 0x00, 0x00, 0x00, 0x00};
        case '-': return {0x08, 0x08, 0x08, 0x08, 0x08};
        case '.': return {0x00, 0x60, 0x60, 0x00, 0x00};
        case ':': return {0x00, 0x36, 0x36, 0x00, 0x00};
        case '/': return {0x20, 0x10, 0x08, 0x04, 0x02};
        case '0': return {0x3E, 0x51, 0x49, 0x45, 0x3E};
        case '1': return {0x00, 0x42, 0x7F, 0x40, 0x00};
        case '2': return {0x42, 0x61, 0x51, 0x49, 0x46};
        case '3': return {0x21, 0x41, 0x45, 0x4B, 0x31};
        case '4': return {0x18, 0x14, 0x12, 0x7F, 0x10};
        case '5': return {0x27, 0x45, 0x45, 0x45, 0x39};
        case '6': return {0x3C, 0x4A, 0x49, 0x49, 0x30};
        case '7': return {0x01, 0x71, 0x09, 0x05, 0x03};
        case '8': return {0x36, 0x49, 0x49, 0x49, 0x36};
        case '9': return {0x06, 0x49, 0x49, 0x29, 0x1E};
        case 'A': return {0x7E, 0x11, 0x11, 0x11, 0x7E};
        case 'B': return {0x7F, 0x49, 0x49, 0x49, 0x36};
        case 'C': return {0x3E, 0x41, 0x41, 0x41, 0x22};
        case 'D': return {0x7F, 0x41, 0x41, 0x22, 0x1C};
        case 'E': return {0x7F, 0x49, 0x49, 0x49, 0x41};
        case 'F': return {0x7F, 0x09, 0x09, 0x09, 0x01};
        case 'G': return {0x3E, 0x41, 0x49, 0x49, 0x7A};
        case 'H': return {0x7F, 0x08, 0x08, 0x08, 0x7F};
        case 'I': return {0x00, 0x41, 0x7F, 0x41, 0x00};
        case 'J': return {0x20, 0x40, 0x41, 0x3F, 0x01};
        case 'K': return {0x7F, 0x08, 0x14, 0x22, 0x41};
        case 'L': return {0x7F, 0x40, 0x40, 0x40, 0x40};
        case 'M': return {0x7F, 0x02, 0x0C, 0x02, 0x7F};
        case 'N': return {0x7F, 0x04, 0x08, 0x10, 0x7F};
        case 'O': return {0x3E, 0x41, 0x41, 0x41, 0x3E};
        case 'P': return {0x7F, 0x09, 0x09, 0x09, 0x06};
        case 'Q': return {0x3E, 0x41, 0x51, 0x21, 0x5E};
        case 'R': return {0x7F, 0x09, 0x19, 0x29, 0x46};
        case 'S': return {0x46, 0x49, 0x49, 0x49, 0x31};
        case 'T': return {0x01, 0x01, 0x7F, 0x01, 0x01};
        case 'U': return {0x3F, 0x40, 0x40, 0x40, 0x3F};
        case 'V': return {0x1F, 0x20, 0x40, 0x20, 0x1F};
        case 'W': return {0x3F, 0x40, 0x38, 0x40, 0x3F};
        case 'X': return {0x63, 0x14, 0x08, 0x14, 0x63};
        case 'Y': return {0x07, 0x08, 0x70, 0x08, 0x07};
        case 'Z': return {0x61, 0x51, 0x49, 0x45, 0x43};
        default: return {0x02, 0x01, 0x51, 0x09, 0x06};
    }
}

}  // namespace

bool Ssd1306Display::init(i2c_inst_t* i2c) {
    i2c_ = i2c;
    available_ = try_init_address(0x3C) || try_init_address(0x3D);
    if (available_) {
        clear();
        set_line(0, "EPS NODE");
        set_line(1, "OLED ONLINE");
        set_line(2, "WAITING CMD");
        flush();
    }
    return available_;
}

bool Ssd1306Display::available() const {
    return available_;
}

uint8_t Ssd1306Display::address() const {
    return address_;
}

void Ssd1306Display::clear() {
    buffer_.fill(0);
}

void Ssd1306Display::set_line(uint8_t line, const char* text) {
    if (line >= kPages || text == nullptr) {
        return;
    }

    const size_t row_offset = static_cast<size_t>(line) * kWidth;
    std::fill(buffer_.begin() + row_offset, buffer_.begin() + row_offset + kWidth, 0);

    size_t col = 0;
    while (*text != '\0' && col + 6 <= kWidth) {
        const Glyph glyph = glyph_for(*text++);
        for (uint8_t i = 0; i < glyph.size(); ++i) {
            buffer_[row_offset + col++] = glyph[i];
        }
        buffer_[row_offset + col++] = 0x00;
    }
}

bool Ssd1306Display::flush() {
    if (!available_) {
        return false;
    }

    for (uint8_t page = 0; page < kPages; ++page) {
        if (!command(static_cast<uint8_t>(0xB0 | page)) ||
            !command(0x00) ||
            !command(0x10)) {
            available_ = false;
            return false;
        }

        const size_t row_offset = static_cast<size_t>(page) * kWidth;
        for (size_t col = 0; col < kWidth; col += 16) {
            const size_t chunk_len = std::min<size_t>(16, kWidth - col);
            if (!write_controlled(&buffer_[row_offset + col], chunk_len, 0x40)) {
                available_ = false;
                return false;
            }
        }
    }
    return true;
}

bool Ssd1306Display::try_init_address(uint8_t address) {
    address_ = address;

    const uint8_t commands[] = {
        0xAE,
        0xD5, 0x80,
        0xA8, 0x1F,
        0xD3, 0x00,
        0x40,
        0x8D, 0x14,
        0x20, 0x02,
        0xA1,
        0xC8,
        0xDA, 0x02,
        0x81, 0x8F,
        0xD9, 0xF1,
        0xDB, 0x40,
        0xA4,
        0xA6,
        0xAF,
    };

    for (size_t i = 0; i < sizeof(commands); ++i) {
        if (!command(commands[i])) {
            address_ = 0;
            return false;
        }
    }

    return true;
}

bool Ssd1306Display::command(uint8_t cmd) {
    return write_controlled(&cmd, 1, 0x00);
}

bool Ssd1306Display::write_controlled(const uint8_t* data, size_t len, uint8_t control) {
    if (i2c_ == nullptr || address_ == 0 || data == nullptr || len > 16) {
        return false;
    }

    uint8_t tx[17] = {};
    tx[0] = control;
    std::memcpy(&tx[1], data, len);
    const int wrote = i2c_write_blocking(i2c_, address_, tx, len + 1, false);
    return wrote == static_cast<int>(len + 1);
}
