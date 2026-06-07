#include <array>
#include <cstddef>
#include <cstdint>

#include "display_status.hpp"
#include "eps_app.hpp"
#include "hardware/uart.h"
#include "hardware/i2c.h"
#include "ina219.hpp"
#include "pico/stdlib.h"
#include "protocol.hpp"
#include "ssd1306.hpp"

using eps_app::EpsNode;
using eps_app::Response;
using eps_protocol::Frame;
using eps_protocol::FrameParser;
using eps_protocol::NodeId;
using eps_protocol::ParseResult;

namespace {

uart_inst_t* const kBusUart = uart0;
constexpr uint32_t kBusBaud = 115200;
constexpr uint kUartTxPin = 0;
constexpr uint kUartRxPin = 1;
constexpr uint kDirectionPin = 2;
constexpr std::array<uint, 4> kLoadPins = {10, 11, 12, 13};
#ifndef EPS_LOAD_ACTIVE_LOW
#define EPS_LOAD_ACTIVE_LOW 0
#endif
constexpr bool kLoadActiveLow = EPS_LOAD_ACTIVE_LOW != 0;
i2c_inst_t* const kPowerI2c = i2c1;
constexpr uint kPowerSdaPin = 6;
constexpr uint kPowerSclPin = 7;
constexpr uint32_t kPowerSamplePeriodMs = 500;
constexpr uint32_t kDisplayPeriodMs = 250;

void apply_loads(uint8_t effective_mask) {
    for (size_t i = 0; i < kLoadPins.size(); ++i) {
        const bool active = (effective_mask & (1u << i)) != 0;
        gpio_put(kLoadPins[i], kLoadActiveLow ? !active : active);
    }
}

void set_bus_receive() {
    gpio_put(kDirectionPin, 0);
}

void set_bus_transmit() {
    gpio_put(kDirectionPin, 1);
}

void send_bus(const uint8_t* data, size_t len) {
    set_bus_transmit();
    sleep_us(20);
    uart_write_blocking(kBusUart, data, len);
    uart_tx_wait_blocking(kBusUart);
    sleep_us(20);
    set_bus_receive();
}

void send_response(const Response& response) {
    if (!response.present) {
        return;
    }

    std::array<uint8_t, eps_protocol::kMaxFrameLen> out{};
    const size_t len = eps_protocol::encode_frame(
        static_cast<uint8_t>(response.msg_type),
        response.payload.data(),
        response.payload_len,
        response.seq,
        static_cast<uint8_t>(NodeId::Eps),
        static_cast<uint8_t>(NodeId::Ground),
        out.data(),
        out.size());
    if (len != 0) {
        send_bus(out.data(), len);
    }
}

void init_load_gpio() {
    for (uint pin : kLoadPins) {
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_OUT);
        gpio_put(pin, kLoadActiveLow ? 1 : 0);
    }
}

void init_rs485_uart() {
    uart_init(kBusUart, kBusBaud);
    gpio_set_function(kUartTxPin, GPIO_FUNC_UART);
    gpio_set_function(kUartRxPin, GPIO_FUNC_UART);
    uart_set_format(kBusUart, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(kBusUart, true);

    gpio_init(kDirectionPin);
    gpio_set_dir(kDirectionPin, GPIO_OUT);
    set_bus_receive();
}

void sample_power(Ina219& power_sensor, EpsNode& node, eps_app::PowerSample& latest_sample) {
    latest_sample = power_sensor.read_sample();
    node.set_power_sample(latest_sample);
}

}  // namespace

int main() {
    init_load_gpio();
    init_rs485_uart();
    Ina219 power_sensor;
    power_sensor.init(kPowerI2c, kPowerSdaPin, kPowerSclPin);
    Ssd1306Display display;
    display.init(kPowerI2c);

    sleep_ms(100);
    EpsNode node;
    node.complete_boot();
    apply_loads(node.effective_load_mask());
    eps_app::PowerSample latest_power_sample;
    sample_power(power_sensor, node, latest_power_sample);
    update_status_display(display, node, latest_power_sample, to_ms_since_boot(get_absolute_time()));
    uint32_t last_power_sample_ms = to_ms_since_boot(get_absolute_time());
    uint32_t last_display_ms = last_power_sample_ms;

    FrameParser parser;
    Frame frame;

    while (true) {
        const uint32_t now_ms = to_ms_since_boot(get_absolute_time());
        if (now_ms - last_power_sample_ms >= kPowerSamplePeriodMs) {
            sample_power(power_sensor, node, latest_power_sample);
            last_power_sample_ms = now_ms;
        }
        if (now_ms - last_display_ms >= kDisplayPeriodMs) {
            update_status_display(display, node, latest_power_sample, now_ms);
            last_display_ms = now_ms;
        }

        while (uart_is_readable(kBusUart)) {
            const uint8_t value = uart_getc(kBusUart);
            const ParseResult result = parser.feed(value, frame);
            if (result == ParseResult::FrameReady) {
                if (frame.msg_type == static_cast<uint8_t>(eps_protocol::MsgType::GetPowerTelemetry)) {
                    sample_power(power_sensor, node, latest_power_sample);
                    last_power_sample_ms = to_ms_since_boot(get_absolute_time());
                }
                const Response response = node.handle_command(
                    frame,
                    to_ms_since_boot(get_absolute_time()));
                apply_loads(node.effective_load_mask());
                send_response(response);
                update_status_display(display, node, latest_power_sample, to_ms_since_boot(get_absolute_time()));
                last_display_ms = to_ms_since_boot(get_absolute_time());
            } else if (result == ParseResult::CrcError) {
                node.note_crc_error();
            }
        }
        tight_loop_contents();
    }
}
