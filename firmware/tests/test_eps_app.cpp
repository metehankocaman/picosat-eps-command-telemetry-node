#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "eps_app.hpp"
#include "protocol.hpp"

namespace {

using eps_app::EpsNode;
using eps_app::Response;
using eps_protocol::Frame;
using eps_protocol::FrameParser;
using eps_protocol::Mode;
using eps_protocol::MsgType;
using eps_protocol::NodeId;
using eps_protocol::ParseResult;
using eps_protocol::Status;

void fail(const char* message) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    std::exit(1);
}

void expect(bool condition, const char* message) {
    if (!condition) {
        fail(message);
    }
}

uint16_t read_u16_le(const std::array<uint8_t, eps_protocol::kMaxPayloadLen>& payload, size_t offset) {
    return static_cast<uint16_t>(payload[offset]) |
        (static_cast<uint16_t>(payload[offset + 1]) << 8);
}

int16_t read_i16_le(const std::array<uint8_t, eps_protocol::kMaxPayloadLen>& payload, size_t offset) {
    return static_cast<int16_t>(read_u16_le(payload, offset));
}

int32_t read_i32_le(const std::array<uint8_t, eps_protocol::kMaxPayloadLen>& payload, size_t offset) {
    return static_cast<int32_t>(
        static_cast<uint32_t>(payload[offset]) |
        (static_cast<uint32_t>(payload[offset + 1]) << 8) |
        (static_cast<uint32_t>(payload[offset + 2]) << 16) |
        (static_cast<uint32_t>(payload[offset + 3]) << 24));
}

Frame request(MsgType msg_type, uint8_t seq, std::initializer_list<uint8_t> payload = {}) {
    Frame frame;
    frame.src = static_cast<uint8_t>(NodeId::Ground);
    frame.dst = static_cast<uint8_t>(NodeId::Eps);
    frame.seq = seq;
    frame.msg_type = static_cast<uint8_t>(msg_type);
    frame.payload_len = static_cast<uint8_t>(payload.size());

    size_t index = 0;
    for (uint8_t byte : payload) {
        frame.payload[index++] = byte;
    }
    return frame;
}

void expect_ack(const Response& response, MsgType request_type, Status status, uint16_t detail) {
    expect(response.present, "expected response");
    expect(
        response.msg_type == (status == Status::Ok ? MsgType::Ack : MsgType::Nack),
        "ACK/NACK message type mismatch");
    expect(response.payload_len == 4, "ACK/NACK payload length mismatch");
    expect(response.payload[0] == static_cast<uint8_t>(request_type), "ACK request type mismatch");
    expect(response.payload[1] == static_cast<uint8_t>(status), "ACK status mismatch");
    expect(read_u16_le(response.payload, 2) == detail, "ACK detail mismatch");
}

void test_protocol_round_trip() {
    std::array<uint8_t, eps_protocol::kMaxFrameLen> encoded{};
    const uint8_t payload[] = {0x03};
    const size_t len = eps_protocol::encode_frame(
        static_cast<uint8_t>(MsgType::SetLoads),
        payload,
        sizeof(payload),
        42,
        static_cast<uint8_t>(NodeId::Ground),
        static_cast<uint8_t>(NodeId::Eps),
        encoded.data(),
        encoded.size());

    expect(len == 11, "encoded frame length mismatch");

    FrameParser parser;
    Frame decoded;
    ParseResult result = ParseResult::None;
    for (size_t i = 0; i < len; ++i) {
        result = parser.feed(encoded[i], decoded);
    }

    expect(result == ParseResult::FrameReady, "parser did not produce frame");
    expect(decoded.seq == 42, "decoded sequence mismatch");
    expect(decoded.msg_type == static_cast<uint8_t>(MsgType::SetLoads), "decoded message mismatch");
    expect(decoded.payload_len == 1, "decoded payload length mismatch");
    expect(decoded.payload[0] == 0x03, "decoded payload mismatch");
}

void test_crc_error_detection() {
    std::array<uint8_t, eps_protocol::kMaxFrameLen> encoded{};
    const size_t len = eps_protocol::encode_frame(
        static_cast<uint8_t>(MsgType::Ping),
        nullptr,
        0,
        7,
        static_cast<uint8_t>(NodeId::Ground),
        static_cast<uint8_t>(NodeId::Eps),
        encoded.data(),
        encoded.size());

    encoded[len - 1] ^= 0x01;

    FrameParser parser;
    Frame decoded;
    ParseResult result = ParseResult::None;
    for (size_t i = 0; i < len; ++i) {
        result = parser.feed(encoded[i], decoded);
    }
    expect(result == ParseResult::CrcError, "parser did not reject bad CRC");
}

void test_eps_safety_flow() {
    EpsNode node;
    node.complete_boot();
    expect(node.mode() == Mode::Nominal, "boot did not complete to NOMINAL");

    Response response = node.handle_command(request(MsgType::SetLoads, 1, {0x03}), 100);
    expect_ack(response, MsgType::SetLoads, Status::Ok, 0);
    expect(node.effective_load_mask() == 0x03, "loads did not turn on in NOMINAL");

    response = node.handle_command(request(MsgType::InjectFault, 2, {0x01, 0x00}), 200);
    expect_ack(response, MsgType::InjectFault, Status::Ok, 1);
    expect(node.mode() == Mode::Safe, "fault did not enter SAFE");
    expect(node.effective_load_mask() == 0x00, "SAFE did not disable loads");

    response = node.handle_command(request(MsgType::SetLoads, 3, {0x0F}), 300);
    expect_ack(response, MsgType::SetLoads, Status::BadState, 1);
    expect(node.mode() == Mode::Safe, "load command in SAFE changed mode");
    expect(node.effective_load_mask() == 0x00, "load command in SAFE enabled loads");

    response = node.handle_command(request(MsgType::RequestNominal, 4), 400);
    expect_ack(response, MsgType::RequestNominal, Status::FaultActive, 1);
    expect(node.mode() == Mode::Safe, "REQUEST_NOMINAL bypassed active fault");

    response = node.handle_command(request(MsgType::ClearFaults, 5), 500);
    expect_ack(response, MsgType::ClearFaults, Status::Ok, 0);
    expect(node.mode() == Mode::Safe, "CLEAR_FAULTS should not leave SAFE by itself");

    response = node.handle_command(request(MsgType::RequestNominal, 6), 600);
    expect_ack(response, MsgType::RequestNominal, Status::Ok, 0);
    expect(node.mode() == Mode::Nominal, "REQUEST_NOMINAL did not recover after fault clear");

    node.note_crc_error();
    response = node.handle_command(request(MsgType::GetTelemetry, 7), 700);
    expect(response.present, "telemetry response missing");
    expect(response.msg_type == MsgType::Telemetry, "telemetry message type mismatch");
    expect(response.payload_len == 14, "telemetry payload length mismatch");
    expect(response.payload[4] == static_cast<uint8_t>(Mode::Nominal), "telemetry mode mismatch");
    expect(read_u16_le(response.payload, 10) == 1, "telemetry CRC error count mismatch");
}

void test_power_telemetry() {
    EpsNode node;
    node.complete_boot();

    eps_app::PowerSample sample;
    sample.sensor_status = eps_app::kPowerSensorPresent;
    sample.bus_mV = 3296;
    sample.shunt_uV = 430;
    sample.current_mA_x10 = 43;
    sample.power_mW = 14;
    node.set_power_sample(sample);

    Response response = node.handle_command(request(MsgType::GetPowerTelemetry, 8), 800);
    expect(response.present, "power telemetry response missing");
    expect(response.msg_type == MsgType::PowerTelemetry, "power telemetry message type mismatch");
    expect(response.payload_len == 17, "power telemetry payload length mismatch");
    expect(response.payload[4] == static_cast<uint8_t>(Mode::Nominal), "power telemetry mode mismatch");
    expect(response.payload[5] == 0x00, "power telemetry load mask mismatch");
    expect(response.payload[6] == eps_app::kPowerSensorPresent, "power telemetry sensor status mismatch");
    expect(read_u16_le(response.payload, 7) == 3296, "power telemetry bus voltage mismatch");
    expect(read_i32_le(response.payload, 9) == 430, "power telemetry shunt voltage mismatch");
    expect(read_i16_le(response.payload, 13) == 43, "power telemetry current mismatch");
    expect(read_u16_le(response.payload, 15) == 14, "power telemetry power mismatch");
}

}  // namespace

int main() {
    test_protocol_round_trip();
    test_crc_error_detection();
    test_eps_safety_flow();
    test_power_telemetry();
    std::puts("C++ EPS app tests passed");
    return 0;
}
