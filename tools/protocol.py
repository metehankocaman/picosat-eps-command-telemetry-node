"""Packet protocol for the PicoSat EPS command/telemetry demo.

The protocol is intentionally small enough to inspect during a 5 minute demo:

    sync(2) + header(6) + payload(N) + crc16(2)

CRC covers the header and payload, not the sync bytes. All multibyte payload
fields are little-endian.
"""

from __future__ import annotations

from dataclasses import dataclass
from enum import IntEnum
import struct
from typing import Iterable


SYNC = b"\xA5\x5A"
VERSION = 1
HEADER_FORMAT = "<BBBBBB"
HEADER_SIZE = struct.calcsize(HEADER_FORMAT)
CRC_SIZE = 2
MAX_PAYLOAD_LEN = 64


class ProtocolError(ValueError):
    """Base class for protocol framing errors."""


class CrcError(ProtocolError):
    """Raised when a frame CRC does not match."""


class NodeId(IntEnum):
    GROUND = 0x01
    EPS = 0x02
    OBC_BRIDGE = 0x03
    BROADCAST = 0xFF


class MsgType(IntEnum):
    PING = 0x01
    GET_TELEMETRY = 0x02
    SET_LOADS = 0x03
    ENTER_SAFE = 0x04
    INJECT_FAULT = 0x05
    CLEAR_FAULTS = 0x06
    REQUEST_NOMINAL = 0x07
    GET_POWER_TELEMETRY = 0x08

    ACK = 0x80
    NACK = 0x81
    TELEMETRY = 0x82
    POWER_TELEMETRY = 0x83


class Mode(IntEnum):
    BOOT = 0
    NOMINAL = 1
    SAFE = 2


class FaultFlag(IntEnum):
    INJECTED = 1 << 0
    BAD_COMMAND = 1 << 1


class Status(IntEnum):
    OK = 0
    BAD_LENGTH = 1
    BAD_STATE = 2
    UNKNOWN_COMMAND = 3
    FAULT_ACTIVE = 4
    CRC_ERROR = 5


ACK_FORMAT = "<BBH"
TELEMETRY_FORMAT = "<IBBHHHBB"
POWER_TELEMETRY_FORMAT = "<IBBBHihH"


class PowerSensorStatus(IntEnum):
    PRESENT = 1 << 0
    I2C_ERROR = 1 << 1
    MATH_OVERFLOW = 1 << 2


@dataclass(frozen=True)
class Frame:
    version: int
    src: int
    dst: int
    seq: int
    msg_type: int
    payload: bytes


@dataclass(frozen=True)
class Ack:
    request_type: int
    status: int
    detail: int


@dataclass(frozen=True)
class Telemetry:
    uptime_ms: int
    mode: int
    load_mask: int
    fault_flags: int
    command_count: int
    crc_error_count: int
    last_command: int
    last_status: int


@dataclass(frozen=True)
class PowerTelemetry:
    uptime_ms: int
    mode: int
    load_mask: int
    sensor_status: int
    bus_mV: int
    shunt_uV: int
    current_mA_x10: int
    power_mW: int


def crc16_ccitt(data: bytes, initial: int = 0xFFFF) -> int:
    """Return CRC-16/CCITT-FALSE for data."""
    crc = initial
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


def _payload_bytes(payload: bytes | bytearray | Iterable[int]) -> bytes:
    if isinstance(payload, (bytes, bytearray)):
        return bytes(payload)
    return bytes(payload)


def encode_frame(
    msg_type: int | MsgType,
    payload: bytes | bytearray | Iterable[int] = b"",
    *,
    seq: int = 0,
    src: int | NodeId = NodeId.GROUND,
    dst: int | NodeId = NodeId.EPS,
) -> bytes:
    payload_bytes = _payload_bytes(payload)
    if len(payload_bytes) > MAX_PAYLOAD_LEN:
        raise ProtocolError(f"payload too large: {len(payload_bytes)} bytes")

    header = struct.pack(
        HEADER_FORMAT,
        VERSION,
        int(src) & 0xFF,
        int(dst) & 0xFF,
        seq & 0xFF,
        int(msg_type) & 0xFF,
        len(payload_bytes),
    )
    crc = crc16_ccitt(header + payload_bytes)
    return SYNC + header + payload_bytes + struct.pack("<H", crc)


def decode_frame(frame: bytes) -> Frame:
    if len(frame) < len(SYNC) + HEADER_SIZE + CRC_SIZE:
        raise ProtocolError("frame is too short")
    if frame[:2] != SYNC:
        raise ProtocolError("missing sync bytes")

    header_start = len(SYNC)
    header_end = header_start + HEADER_SIZE
    version, src, dst, seq, msg_type, payload_len = struct.unpack(
        HEADER_FORMAT, frame[header_start:header_end]
    )
    if version != VERSION:
        raise ProtocolError(f"unsupported protocol version: {version}")
    if payload_len > MAX_PAYLOAD_LEN:
        raise ProtocolError(f"payload length exceeds limit: {payload_len}")

    expected_len = len(SYNC) + HEADER_SIZE + payload_len + CRC_SIZE
    if len(frame) != expected_len:
        raise ProtocolError(f"frame length mismatch: expected {expected_len}, got {len(frame)}")

    payload = frame[header_end : header_end + payload_len]
    received_crc = struct.unpack("<H", frame[-CRC_SIZE:])[0]
    computed_crc = crc16_ccitt(frame[header_start:-CRC_SIZE])
    if received_crc != computed_crc:
        raise CrcError(
            f"crc mismatch: received 0x{received_crc:04X}, computed 0x{computed_crc:04X}"
        )

    return Frame(version, src, dst, seq, msg_type, payload)


class PacketStream:
    """Incremental packet parser for serial byte streams."""

    def __init__(self) -> None:
        self._buffer = bytearray()
        self.crc_errors = 0
        self.format_errors = 0

    def feed(self, data: bytes) -> list[Frame]:
        self._buffer.extend(data)
        frames: list[Frame] = []

        while True:
            sync_index = self._buffer.find(SYNC)
            if sync_index < 0:
                if self._buffer:
                    self._buffer.clear()
                    self.format_errors += 1
                return frames
            if sync_index:
                del self._buffer[:sync_index]
                self.format_errors += 1

            minimum = len(SYNC) + HEADER_SIZE + CRC_SIZE
            if len(self._buffer) < minimum:
                return frames

            payload_len = self._buffer[len(SYNC) + HEADER_SIZE - 1]
            if payload_len > MAX_PAYLOAD_LEN:
                del self._buffer[0]
                self.format_errors += 1
                continue

            frame_len = len(SYNC) + HEADER_SIZE + payload_len + CRC_SIZE
            if len(self._buffer) < frame_len:
                return frames

            raw = bytes(self._buffer[:frame_len])
            del self._buffer[:frame_len]
            try:
                frames.append(decode_frame(raw))
            except CrcError:
                self.crc_errors += 1
            except ProtocolError:
                self.format_errors += 1

        return frames


def pack_ack(request_type: int | MsgType, status: int | Status, detail: int = 0) -> bytes:
    return struct.pack(ACK_FORMAT, int(request_type) & 0xFF, int(status) & 0xFF, detail & 0xFFFF)


def unpack_ack(payload: bytes) -> Ack:
    if len(payload) != struct.calcsize(ACK_FORMAT):
        raise ProtocolError(f"ACK payload length mismatch: {len(payload)}")
    return Ack(*struct.unpack(ACK_FORMAT, payload))


def pack_telemetry(
    *,
    uptime_ms: int,
    mode: int | Mode,
    load_mask: int,
    fault_flags: int,
    command_count: int,
    crc_error_count: int,
    last_command: int,
    last_status: int | Status,
) -> bytes:
    return struct.pack(
        TELEMETRY_FORMAT,
        uptime_ms & 0xFFFFFFFF,
        int(mode) & 0xFF,
        load_mask & 0xFF,
        fault_flags & 0xFFFF,
        command_count & 0xFFFF,
        crc_error_count & 0xFFFF,
        last_command & 0xFF,
        int(last_status) & 0xFF,
    )


def unpack_telemetry(payload: bytes) -> Telemetry:
    if len(payload) != struct.calcsize(TELEMETRY_FORMAT):
        raise ProtocolError(f"telemetry payload length mismatch: {len(payload)}")
    return Telemetry(*struct.unpack(TELEMETRY_FORMAT, payload))


def pack_power_telemetry(
    *,
    uptime_ms: int,
    mode: int | Mode,
    load_mask: int,
    sensor_status: int,
    bus_mV: int,
    shunt_uV: int,
    current_mA_x10: int,
    power_mW: int,
) -> bytes:
    return struct.pack(
        POWER_TELEMETRY_FORMAT,
        uptime_ms & 0xFFFFFFFF,
        int(mode) & 0xFF,
        load_mask & 0xFF,
        sensor_status & 0xFF,
        bus_mV & 0xFFFF,
        shunt_uV,
        current_mA_x10,
        power_mW & 0xFFFF,
    )


def unpack_power_telemetry(payload: bytes) -> PowerTelemetry:
    if len(payload) != struct.calcsize(POWER_TELEMETRY_FORMAT):
        raise ProtocolError(f"power telemetry payload length mismatch: {len(payload)}")
    return PowerTelemetry(*struct.unpack(POWER_TELEMETRY_FORMAT, payload))


def pack_set_loads(load_mask: int) -> bytes:
    return struct.pack("<B", load_mask & 0xFF)


def unpack_set_loads(payload: bytes) -> int:
    if len(payload) != 1:
        raise ProtocolError(f"SET_LOADS payload length mismatch: {len(payload)}")
    return payload[0]


def pack_fault_flags(flags: int | FaultFlag) -> bytes:
    return struct.pack("<H", int(flags) & 0xFFFF)


def unpack_fault_flags(payload: bytes) -> int:
    if len(payload) != 2:
        raise ProtocolError(f"fault flag payload length mismatch: {len(payload)}")
    return struct.unpack("<H", payload)[0]


def name_for_msg(msg_type: int) -> str:
    try:
        return MsgType(msg_type).name
    except ValueError:
        return f"UNKNOWN_0x{msg_type:02X}"


def name_for_mode(mode: int) -> str:
    try:
        return Mode(mode).name
    except ValueError:
        return f"UNKNOWN_0x{mode:02X}"


def name_for_status(status: int) -> str:
    try:
        return Status(status).name
    except ValueError:
        return f"UNKNOWN_0x{status:02X}"
