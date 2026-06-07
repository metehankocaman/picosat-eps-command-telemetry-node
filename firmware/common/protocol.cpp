#include "protocol.hpp"

namespace eps_protocol {

namespace {

uint16_t read_u16_le(const uint8_t* data) {
    return static_cast<uint16_t>(data[0]) | (static_cast<uint16_t>(data[1]) << 8);
}

void write_u16_le(uint8_t* data, uint16_t value) {
    data[0] = static_cast<uint8_t>(value & 0xFF);
    data[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
}

}  // namespace

uint16_t crc16_ccitt(const uint8_t* data, size_t len, uint16_t initial) {
    uint16_t crc = initial;
    for (size_t i = 0; i < len; ++i) {
        crc ^= static_cast<uint16_t>(data[i]) << 8;
        for (int bit = 0; bit < 8; ++bit) {
            if ((crc & 0x8000u) != 0) {
                crc = static_cast<uint16_t>((crc << 1) ^ 0x1021u);
            } else {
                crc = static_cast<uint16_t>(crc << 1);
            }
        }
    }
    return crc;
}

size_t encode_frame(
    uint8_t msg_type,
    const uint8_t* payload,
    size_t payload_len,
    uint8_t seq,
    uint8_t src,
    uint8_t dst,
    uint8_t* out,
    size_t out_capacity) {
    if (payload_len > kMaxPayloadLen) {
        return 0;
    }
    const size_t frame_len = 2 + kHeaderSize + payload_len + kCrcSize;
    if (out_capacity < frame_len) {
        return 0;
    }

    out[0] = kSync0;
    out[1] = kSync1;
    out[2] = kVersion;
    out[3] = src;
    out[4] = dst;
    out[5] = seq;
    out[6] = msg_type;
    out[7] = static_cast<uint8_t>(payload_len);

    for (size_t i = 0; i < payload_len; ++i) {
        out[8 + i] = payload[i];
    }

    const uint16_t crc = crc16_ccitt(&out[2], kHeaderSize + payload_len);
    write_u16_le(&out[8 + payload_len], crc);
    return frame_len;
}

void FrameParser::reset() {
    len_ = 0;
}

ParseResult FrameParser::feed(uint8_t byte, Frame& out) {
    if (len_ >= buffer_.size()) {
        reset();
        return ParseResult::FormatError;
    }

    buffer_[len_++] = byte;

    if (len_ == 1) {
        if (buffer_[0] != kSync0) {
            reset();
            return ParseResult::FormatError;
        }
        return ParseResult::None;
    }

    if (len_ == 2) {
        if (buffer_[1] != kSync1) {
            const bool possible_new_sync = buffer_[1] == kSync0;
            reset();
            if (possible_new_sync) {
                buffer_[0] = kSync0;
                len_ = 1;
            }
            return ParseResult::FormatError;
        }
        return ParseResult::None;
    }

    if (len_ < 2 + kHeaderSize) {
        return ParseResult::None;
    }

    const uint8_t version = buffer_[2];
    const uint8_t payload_len = buffer_[7];
    if (version != kVersion || payload_len > kMaxPayloadLen) {
        reset();
        return ParseResult::FormatError;
    }

    const size_t expected_len = 2 + kHeaderSize + payload_len + kCrcSize;
    if (len_ < expected_len) {
        return ParseResult::None;
    }

    const uint16_t received_crc = read_u16_le(&buffer_[expected_len - kCrcSize]);
    const uint16_t computed_crc = crc16_ccitt(&buffer_[2], kHeaderSize + payload_len);
    if (received_crc != computed_crc) {
        reset();
        return ParseResult::CrcError;
    }

    out.version = version;
    out.src = buffer_[3];
    out.dst = buffer_[4];
    out.seq = buffer_[5];
    out.msg_type = buffer_[6];
    out.payload_len = payload_len;
    for (size_t i = 0; i < payload_len; ++i) {
        out.payload[i] = buffer_[8 + i];
    }

    reset();
    return ParseResult::FrameReady;
}

}  // namespace eps_protocol
