#include "eps_app.hpp"

namespace eps_app {

namespace {

constexpr uint8_t kAllLoadsMask = 0x0F;

void write_u16_le(uint8_t* out, uint16_t value) {
    out[0] = static_cast<uint8_t>(value & 0xFF);
    out[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
}

void write_u32_le(uint8_t* out, uint32_t value) {
    out[0] = static_cast<uint8_t>(value & 0xFF);
    out[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
    out[2] = static_cast<uint8_t>((value >> 16) & 0xFF);
    out[3] = static_cast<uint8_t>((value >> 24) & 0xFF);
}

void write_i16_le(uint8_t* out, int16_t value) {
    write_u16_le(out, static_cast<uint16_t>(value));
}

void write_i32_le(uint8_t* out, int32_t value) {
    write_u32_le(out, static_cast<uint32_t>(value));
}

uint16_t read_u16_le(const uint8_t* in) {
    return static_cast<uint16_t>(in[0]) | (static_cast<uint16_t>(in[1]) << 8);
}

}  // namespace

void EpsNode::complete_boot() {
    mode_ = eps_protocol::Mode::Nominal;
}

void EpsNode::note_crc_error() {
    crc_error_count_++;
}

void EpsNode::set_power_sample(const PowerSample& sample) {
    power_sample_ = sample;
}

eps_protocol::Mode EpsNode::mode() const {
    return mode_;
}

uint8_t EpsNode::commanded_load_mask() const {
    return load_mask_ & kAllLoadsMask;
}

uint8_t EpsNode::effective_load_mask() const {
    return mode_ == eps_protocol::Mode::Safe ? 0 : commanded_load_mask();
}

uint16_t EpsNode::fault_flags() const {
    return fault_flags_;
}

uint16_t EpsNode::command_count() const {
    return command_count_;
}

uint16_t EpsNode::crc_error_count() const {
    return crc_error_count_;
}

uint8_t EpsNode::last_command() const {
    return last_command_;
}

eps_protocol::Status EpsNode::last_status() const {
    return last_status_;
}

void EpsNode::force_safe() {
    mode_ = eps_protocol::Mode::Safe;
    load_mask_ = 0;
}

Response EpsNode::make_ack(
    const eps_protocol::Frame& request,
    eps_protocol::Status status,
    uint16_t detail) {
    Response response;
    response.present = true;
    response.msg_type = status == eps_protocol::Status::Ok
        ? eps_protocol::MsgType::Ack
        : eps_protocol::MsgType::Nack;
    response.seq = request.seq;
    response.payload_len = 4;
    response.payload[0] = request.msg_type;
    response.payload[1] = static_cast<uint8_t>(status);
    write_u16_le(&response.payload[2], detail);
    last_status_ = status;
    return response;
}

Response EpsNode::make_telemetry(uint8_t seq, uint32_t uptime_ms) {
    last_status_ = eps_protocol::Status::Ok;

    Response response;
    response.present = true;
    response.msg_type = eps_protocol::MsgType::Telemetry;
    response.seq = seq;
    response.payload_len = 14;

    write_u32_le(&response.payload[0], uptime_ms);
    response.payload[4] = static_cast<uint8_t>(mode_);
    response.payload[5] = effective_load_mask();
    write_u16_le(&response.payload[6], fault_flags_);
    write_u16_le(&response.payload[8], command_count_);
    write_u16_le(&response.payload[10], crc_error_count_);
    response.payload[12] = last_command_;
    response.payload[13] = static_cast<uint8_t>(last_status_);
    return response;
}

Response EpsNode::make_power_telemetry(uint8_t seq, uint32_t uptime_ms) {
    last_status_ = eps_protocol::Status::Ok;

    Response response;
    response.present = true;
    response.msg_type = eps_protocol::MsgType::PowerTelemetry;
    response.seq = seq;
    response.payload_len = 17;

    write_u32_le(&response.payload[0], uptime_ms);
    response.payload[4] = static_cast<uint8_t>(mode_);
    response.payload[5] = effective_load_mask();
    response.payload[6] = power_sample_.sensor_status;
    write_u16_le(&response.payload[7], power_sample_.bus_mV);
    write_i32_le(&response.payload[9], power_sample_.shunt_uV);
    write_i16_le(&response.payload[13], power_sample_.current_mA_x10);
    write_u16_le(&response.payload[15], power_sample_.power_mW);
    return response;
}

bool EpsNode::payload_len_is(const eps_protocol::Frame& request, uint8_t expected, Response& response) {
    if (request.payload_len == expected) {
        return true;
    }
    response = make_ack(request, eps_protocol::Status::BadLength, request.payload_len);
    return false;
}

Response EpsNode::handle_command(const eps_protocol::Frame& request, uint32_t uptime_ms) {
    if (request.dst != static_cast<uint8_t>(eps_protocol::NodeId::Eps) &&
        request.dst != static_cast<uint8_t>(eps_protocol::NodeId::Broadcast)) {
        return {};
    }

    command_count_++;
    last_command_ = request.msg_type;

    Response response;
    switch (static_cast<eps_protocol::MsgType>(request.msg_type)) {
        case eps_protocol::MsgType::Ping:
            if (payload_len_is(request, 0, response)) {
                response = make_ack(request, eps_protocol::Status::Ok);
            }
            break;

        case eps_protocol::MsgType::GetTelemetry:
            if (payload_len_is(request, 0, response)) {
                response = make_telemetry(request.seq, uptime_ms);
            }
            break;

        case eps_protocol::MsgType::SetLoads:
            if (!payload_len_is(request, 1, response)) {
                break;
            }
            if (mode_ == eps_protocol::Mode::Safe || fault_flags_ != 0) {
                force_safe();
                response = make_ack(request, eps_protocol::Status::BadState, fault_flags_);
                break;
            }
            load_mask_ = request.payload[0] & kAllLoadsMask;
            response = make_ack(request, eps_protocol::Status::Ok);
            break;

        case eps_protocol::MsgType::EnterSafe:
            if (payload_len_is(request, 0, response)) {
                force_safe();
                response = make_ack(request, eps_protocol::Status::Ok);
            }
            break;

        case eps_protocol::MsgType::InjectFault:
            if (!payload_len_is(request, 2, response)) {
                break;
            }
            fault_flags_ |= read_u16_le(request.payload.data());
            if (fault_flags_ == 0) {
                fault_flags_ = eps_protocol::kFaultInjected;
            }
            force_safe();
            response = make_ack(request, eps_protocol::Status::Ok, fault_flags_);
            break;

        case eps_protocol::MsgType::ClearFaults:
            if (payload_len_is(request, 0, response)) {
                fault_flags_ = 0;
                response = make_ack(request, eps_protocol::Status::Ok);
            }
            break;

        case eps_protocol::MsgType::RequestNominal:
            if (!payload_len_is(request, 0, response)) {
                break;
            }
            if (fault_flags_ == 0) {
                mode_ = eps_protocol::Mode::Nominal;
                response = make_ack(request, eps_protocol::Status::Ok);
            } else {
                force_safe();
                response = make_ack(request, eps_protocol::Status::FaultActive, fault_flags_);
            }
            break;

        case eps_protocol::MsgType::GetPowerTelemetry:
            if (payload_len_is(request, 0, response)) {
                response = make_power_telemetry(request.seq, uptime_ms);
            }
            break;

        default:
            response = make_ack(request, eps_protocol::Status::UnknownCommand, request.msg_type);
            break;
    }

    return response;
}

}  // namespace eps_app
