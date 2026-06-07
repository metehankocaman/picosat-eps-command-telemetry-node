#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace eps_protocol {

constexpr uint8_t kSync0 = 0xA5;
constexpr uint8_t kSync1 = 0x5A;
constexpr uint8_t kVersion = 1;
constexpr size_t kHeaderSize = 6;
constexpr size_t kCrcSize = 2;
constexpr size_t kMaxPayloadLen = 64;
constexpr size_t kMaxFrameLen = 2 + kHeaderSize + kMaxPayloadLen + kCrcSize;

enum class NodeId : uint8_t {
    Ground = 0x01,
    Eps = 0x02,
    ObcBridge = 0x03,
    Broadcast = 0xFF,
};

enum class MsgType : uint8_t {
    Ping = 0x01,
    GetTelemetry = 0x02,
    SetLoads = 0x03,
    EnterSafe = 0x04,
    InjectFault = 0x05,
    ClearFaults = 0x06,
    RequestNominal = 0x07,
    GetPowerTelemetry = 0x08,
    Ack = 0x80,
    Nack = 0x81,
    Telemetry = 0x82,
    PowerTelemetry = 0x83,
};

enum class Mode : uint8_t {
    Boot = 0,
    Nominal = 1,
    Safe = 2,
};

enum FaultFlag : uint16_t {
    kFaultInjected = 1u << 0,
    kFaultBadCommand = 1u << 1,
};

enum class Status : uint8_t {
    Ok = 0,
    BadLength = 1,
    BadState = 2,
    UnknownCommand = 3,
    FaultActive = 4,
    CrcError = 5,
};

struct Frame {
    uint8_t version = kVersion;
    uint8_t src = 0;
    uint8_t dst = 0;
    uint8_t seq = 0;
    uint8_t msg_type = 0;
    uint8_t payload_len = 0;
    std::array<uint8_t, kMaxPayloadLen> payload{};
};

enum class ParseResult {
    None,
    FrameReady,
    CrcError,
    FormatError,
};

uint16_t crc16_ccitt(const uint8_t* data, size_t len, uint16_t initial = 0xFFFF);

size_t encode_frame(
    uint8_t msg_type,
    const uint8_t* payload,
    size_t payload_len,
    uint8_t seq,
    uint8_t src,
    uint8_t dst,
    uint8_t* out,
    size_t out_capacity);

class FrameParser {
public:
    ParseResult feed(uint8_t byte, Frame& out);
    void reset();

private:
    std::array<uint8_t, kMaxFrameLen> buffer_{};
    size_t len_ = 0;
};

}  // namespace eps_protocol
