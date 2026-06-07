import struct
import sys
import time

from machine import Pin

SYNC = b"\xA5\x5A"
VERSION = 1
MAX_PAYLOAD_LEN = 64

NODE_GROUND = 0x01
NODE_EPS = 0x02
NODE_BROADCAST = 0xFF

PING = 0x01
GET_TELEMETRY = 0x02
SET_LOADS = 0x03
ENTER_SAFE = 0x04
INJECT_FAULT = 0x05
CLEAR_FAULTS = 0x06
REQUEST_NOMINAL = 0x07

ACK = 0x80
NACK = 0x81
TELEMETRY = 0x82

BOOT = 0
NOMINAL = 1
SAFE = 2

FAULT_INJECTED = 1 << 0

STATUS_OK = 0
STATUS_BAD_LENGTH = 1
STATUS_BAD_STATE = 2
STATUS_UNKNOWN_COMMAND = 3
STATUS_FAULT_ACTIVE = 4

LOAD_PINS = [Pin(pin, Pin.OUT, value=0) for pin in (10, 11, 12, 13)]
ALL_LOADS_MASK = 0x0F
USB_IN = getattr(sys.stdin, "buffer", sys.stdin)
USB_OUT = getattr(sys.stdout, "buffer", sys.stdout)

mode = BOOT
load_mask = 0
fault_flags = 0
command_count = 0
crc_error_count = 0
last_command = 0
last_status = STATUS_OK


def crc16_ccitt(data, initial=0xFFFF):
    crc = initial
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


def apply_loads():
    effective = 0 if mode == SAFE else (load_mask & ALL_LOADS_MASK)
    for index, pin in enumerate(LOAD_PINS):
        pin.value(1 if effective & (1 << index) else 0)


def force_safe():
    global mode, load_mask
    mode = SAFE
    load_mask = 0
    apply_loads()


def encode_frame(msg_type, payload=b"", seq=0, src=NODE_EPS, dst=NODE_GROUND):
    header = struct.pack("<BBBBBB", VERSION, src, dst, seq & 0xFF, msg_type, len(payload))
    crc = crc16_ccitt(header + payload)
    return SYNC + header + payload + struct.pack("<H", crc)


def put_u16(out, offset, value):
    out[offset] = value & 0xFF
    out[offset + 1] = (value >> 8) & 0xFF


def put_u32(out, offset, value):
    out[offset] = value & 0xFF
    out[offset + 1] = (value >> 8) & 0xFF
    out[offset + 2] = (value >> 16) & 0xFF
    out[offset + 3] = (value >> 24) & 0xFF


def hex_encode(data):
    chars = "0123456789ABCDEF"
    out = []
    for byte in data:
        out.append(chars[(byte >> 4) & 0x0F])
        out.append(chars[byte & 0x0F])
    return "".join(out)


def hex_decode(line):
    line = line.strip()
    if len(line) % 2:
        return None
    out = bytearray()
    for i in range(0, len(line), 2):
        try:
            out.append(int(line[i : i + 2], 16))
        except ValueError:
            return None
    return bytes(out)


def write_frame(msg_type, payload=b"", seq=0, use_hex=False):
    frame = encode_frame(msg_type, payload, seq=seq)
    if use_hex:
        USB_OUT.write(hex_encode(frame) + "\n")
        flush = getattr(USB_OUT, "flush", None)
        if flush:
            flush()
        return
    try:
        USB_OUT.write(frame)
    except TypeError:
        USB_OUT.write(frame.decode("latin-1"))
    flush = getattr(USB_OUT, "flush", None)
    if flush:
        flush()


def send_ack(request, status, detail=0, use_hex=False):
    global last_status
    last_status = status
    payload = struct.pack("<BBH", request["msg_type"], status, detail & 0xFFFF)
    write_frame(ACK if status == STATUS_OK else NACK, payload, request["seq"], use_hex)


def send_telemetry(seq, use_hex=False):
    global last_status
    last_status = STATUS_OK
    payload = bytearray(14)
    put_u32(payload, 0, time.ticks_ms() & 0xFFFFFFFF)
    payload[4] = mode & 0xFF
    payload[5] = (0 if mode == SAFE else (load_mask & ALL_LOADS_MASK)) & 0xFF
    put_u16(payload, 6, fault_flags & 0xFFFF)
    put_u16(payload, 8, command_count & 0xFFFF)
    put_u16(payload, 10, crc_error_count & 0xFFFF)
    payload[12] = last_command & 0xFF
    payload[13] = last_status & 0xFF
    write_frame(TELEMETRY, payload, seq, use_hex)


def payload_len_is(request, expected, use_hex=False):
    if len(request["payload"]) == expected:
        return True
    send_ack(request, STATUS_BAD_LENGTH, len(request["payload"]), use_hex)
    return False


class Parser:
    def __init__(self):
        self.buf = bytearray()

    def feed(self, byte):
        self.buf.append(byte)
        if len(self.buf) == 1 and self.buf[0] != SYNC[0]:
            self.buf.clear()
            return None, "format"
        if len(self.buf) == 2 and self.buf[1] != SYNC[1]:
            keep_sync = self.buf[1] == SYNC[0]
            self.buf.clear()
            if keep_sync:
                self.buf.append(SYNC[0])
            return None, "format"
        if len(self.buf) < 8:
            return None, None

        version, src, dst, seq, msg_type, payload_len = struct.unpack("<BBBBBB", self.buf[2:8])
        if version != VERSION or payload_len > MAX_PAYLOAD_LEN:
            self.buf.clear()
            return None, "format"

        frame_len = 2 + 6 + payload_len + 2
        if len(self.buf) < frame_len:
            return None, None

        raw = bytes(self.buf[:frame_len])
        del self.buf[:frame_len]
        received_crc = struct.unpack("<H", raw[-2:])[0]
        computed_crc = crc16_ccitt(raw[2:-2])
        if received_crc != computed_crc:
            return None, "crc"

        return {
            "src": src,
            "dst": dst,
            "seq": seq,
            "msg_type": msg_type,
            "payload": raw[8:-2],
        }, None


def decode_raw_frame(raw):
    if len(raw) < 10 or raw[0:2] != SYNC:
        return None, "format"
    version, src, dst, seq, msg_type, payload_len = struct.unpack("<BBBBBB", raw[2:8])
    if version != VERSION or payload_len > MAX_PAYLOAD_LEN:
        return None, "format"
    expected_len = 2 + 6 + payload_len + 2
    if len(raw) != expected_len:
        return None, "format"
    received_crc = struct.unpack("<H", raw[-2:])[0]
    computed_crc = crc16_ccitt(raw[2:-2])
    if received_crc != computed_crc:
        return None, "crc"
    return {
        "src": src,
        "dst": dst,
        "seq": seq,
        "msg_type": msg_type,
        "payload": raw[8:-2],
    }, None


def handle_frame(request, use_hex=False):
    global command_count, last_command, load_mask, fault_flags, mode

    if request["dst"] not in (NODE_EPS, NODE_BROADCAST):
        return

    command_count = (command_count + 1) & 0xFFFF
    last_command = request["msg_type"]
    msg_type = request["msg_type"]
    payload = request["payload"]

    if msg_type == PING:
        if payload_len_is(request, 0, use_hex):
            send_ack(request, STATUS_OK, use_hex=use_hex)
    elif msg_type == GET_TELEMETRY:
        if payload_len_is(request, 0, use_hex):
            send_telemetry(request["seq"], use_hex)
    elif msg_type == SET_LOADS:
        if not payload_len_is(request, 1, use_hex):
            return
        if mode == SAFE or fault_flags:
            force_safe()
            send_ack(request, STATUS_BAD_STATE, fault_flags, use_hex)
            return
        load_mask = payload[0] & ALL_LOADS_MASK
        apply_loads()
        send_ack(request, STATUS_OK, use_hex=use_hex)
    elif msg_type == ENTER_SAFE:
        if payload_len_is(request, 0, use_hex):
            force_safe()
            send_ack(request, STATUS_OK, use_hex=use_hex)
    elif msg_type == INJECT_FAULT:
        if not payload_len_is(request, 2, use_hex):
            return
        flags = struct.unpack("<H", payload)[0] or FAULT_INJECTED
        fault_flags |= flags
        force_safe()
        send_ack(request, STATUS_OK, fault_flags, use_hex)
    elif msg_type == CLEAR_FAULTS:
        if payload_len_is(request, 0, use_hex):
            fault_flags = 0
            send_ack(request, STATUS_OK, use_hex=use_hex)
    elif msg_type == REQUEST_NOMINAL:
        if not payload_len_is(request, 0, use_hex):
            return
        if fault_flags == 0:
            mode = NOMINAL
            apply_loads()
            send_ack(request, STATUS_OK, use_hex=use_hex)
        else:
            force_safe()
            send_ack(request, STATUS_FAULT_ACTIVE, fault_flags, use_hex)
    else:
        send_ack(request, STATUS_UNKNOWN_COMMAND, msg_type, use_hex)


def main():
    global mode, crc_error_count
    mode = NOMINAL
    apply_loads()

    while True:
        try:
            line = input()
        except EOFError:
            time.sleep_ms(10)
            continue
        except KeyboardInterrupt:
            raise

        if line.strip().upper() == "STOP":
            print("EPS demo stopped; returning to REPL")
            return

        raw = hex_decode(line)
        if raw is None:
            continue

        frame, error = decode_raw_frame(raw)
        if error == "crc":
            crc_error_count = (crc_error_count + 1) & 0xFFFF
        elif frame:
            handle_frame(frame, use_hex=True)


main()
