#include <array>
#include <cstddef>
#include <cstdint>

#include "hardware/uart.h"
#include "pico/stdlib.h"
#include "protocol.hpp"

namespace {

uart_inst_t* const kBusUart = uart0;
constexpr uint32_t kBusBaud = 115200;
constexpr uint kUartTxPin = 0;
constexpr uint kUartRxPin = 1;
constexpr uint kDirectionPin = 2;

struct CollectedFrame {
    std::array<uint8_t, eps_protocol::kMaxFrameLen> bytes{};
    size_t len = 0;
};

class WireFrameCollector {
public:
    bool feed(uint8_t byte, CollectedFrame& out) {
        if (len_ >= buffer_.size()) {
            reset();
        }

        buffer_[len_++] = byte;

        if (len_ == 1) {
            if (buffer_[0] != eps_protocol::kSync0) {
                reset();
            }
            return false;
        }

        if (len_ == 2) {
            if (buffer_[1] != eps_protocol::kSync1) {
                const bool possible_new_sync = buffer_[1] == eps_protocol::kSync0;
                reset();
                if (possible_new_sync) {
                    buffer_[0] = eps_protocol::kSync0;
                    len_ = 1;
                }
            }
            return false;
        }

        if (len_ < 2 + eps_protocol::kHeaderSize) {
            return false;
        }

        const uint8_t version = buffer_[2];
        const uint8_t payload_len = buffer_[7];
        if (version != eps_protocol::kVersion || payload_len > eps_protocol::kMaxPayloadLen) {
            reset();
            return false;
        }

        const size_t expected_len = 2 + eps_protocol::kHeaderSize + payload_len + eps_protocol::kCrcSize;
        if (len_ < expected_len) {
            return false;
        }

        out.len = expected_len;
        for (size_t i = 0; i < expected_len; ++i) {
            out.bytes[i] = buffer_[i];
        }
        reset();
        return true;
    }

private:
    std::array<uint8_t, eps_protocol::kMaxFrameLen> buffer_{};
    size_t len_ = 0;

    void reset() {
        len_ = 0;
    }
};

void set_bus_receive() {
    gpio_put(kDirectionPin, 0);
}

void set_bus_transmit() {
    gpio_put(kDirectionPin, 1);
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

void send_usb(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        putchar_raw(data[i]);
    }
}

void send_bus(const uint8_t* data, size_t len) {
    set_bus_transmit();
    sleep_us(20);
    uart_write_blocking(kBusUart, data, len);
    uart_tx_wait_blocking(kBusUart);
    sleep_us(20);
    set_bus_receive();
}

}  // namespace

int main() {
    stdio_init_all();
    init_rs485_uart();

    sleep_ms(1000);

    WireFrameCollector usb_frames;
    WireFrameCollector bus_frames;
    CollectedFrame frame;

    while (true) {
        int usb_value = getchar_timeout_us(0);
        while (usb_value != PICO_ERROR_TIMEOUT) {
            if (usb_frames.feed(static_cast<uint8_t>(usb_value), frame)) {
                send_bus(frame.bytes.data(), frame.len);
            }
            usb_value = getchar_timeout_us(0);
        }

        while (uart_is_readable(kBusUart)) {
            const uint8_t bus_value = uart_getc(kBusUart);
            if (bus_frames.feed(bus_value, frame)) {
                send_usb(frame.bytes.data(), frame.len);
            }
        }

        tight_loop_contents();
    }
}
