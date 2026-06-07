#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "protocol.hpp"

namespace eps_app {

struct Response {
    bool present = false;
    eps_protocol::MsgType msg_type = eps_protocol::MsgType::Ack;
    uint8_t seq = 0;
    std::array<uint8_t, eps_protocol::kMaxPayloadLen> payload{};
    size_t payload_len = 0;
};

enum PowerSensorStatus : uint8_t {
    kPowerSensorPresent = 1u << 0,
    kPowerSensorI2cError = 1u << 1,
    kPowerSensorMathOverflow = 1u << 2,
};

struct PowerSample {
    uint8_t sensor_status = kPowerSensorI2cError;
    uint16_t bus_mV = 0;
    int32_t shunt_uV = 0;
    int16_t current_mA_x10 = 0;
    uint16_t power_mW = 0;
};

class EpsNode {
public:
    void complete_boot();
    void note_crc_error();
    void set_power_sample(const PowerSample& sample);

    Response handle_command(const eps_protocol::Frame& request, uint32_t uptime_ms);

    eps_protocol::Mode mode() const;
    uint8_t commanded_load_mask() const;
    uint8_t effective_load_mask() const;
    uint16_t fault_flags() const;
    uint16_t command_count() const;
    uint16_t crc_error_count() const;
    uint8_t last_command() const;
    eps_protocol::Status last_status() const;

private:
    eps_protocol::Mode mode_ = eps_protocol::Mode::Boot;
    uint8_t load_mask_ = 0;
    uint16_t fault_flags_ = 0;
    uint16_t command_count_ = 0;
    uint16_t crc_error_count_ = 0;
    uint8_t last_command_ = 0;
    eps_protocol::Status last_status_ = eps_protocol::Status::Ok;
    PowerSample power_sample_{};

    void force_safe();
    Response make_ack(const eps_protocol::Frame& request, eps_protocol::Status status, uint16_t detail = 0);
    Response make_telemetry(uint8_t seq, uint32_t uptime_ms);
    Response make_power_telemetry(uint8_t seq, uint32_t uptime_ms);
    bool payload_len_is(const eps_protocol::Frame& request, uint8_t expected, Response& response);
};

}  // namespace eps_app
