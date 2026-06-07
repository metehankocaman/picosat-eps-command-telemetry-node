import pathlib
import sys

import pytest


PROJECT_ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT_ROOT / "tools"))

from protocol import (  # noqa: E402
    CrcError,
    FaultFlag,
    Frame,
    Mode,
    MsgType,
    NodeId,
    PacketStream,
    PowerSensorStatus,
    ProtocolError,
    Status,
    crc16_ccitt,
    decode_frame,
    encode_frame,
    name_for_msg,
    pack_ack,
    pack_fault_flags,
    pack_power_telemetry,
    pack_set_loads,
    pack_telemetry,
    unpack_ack,
    unpack_fault_flags,
    unpack_power_telemetry,
    unpack_set_loads,
    unpack_telemetry,
)


def test_crc16_ccitt_known_vector() -> None:
    assert crc16_ccitt(b"123456789") == 0x29B1


def test_encode_decode_ping_frame() -> None:
    raw = encode_frame(MsgType.PING, seq=7)
    frame = decode_frame(raw)

    assert frame == Frame(
        version=1,
        src=NodeId.GROUND,
        dst=NodeId.EPS,
        seq=7,
        msg_type=MsgType.PING,
        payload=b"",
    )


def test_set_loads_payload_round_trip() -> None:
    payload = pack_set_loads(0b1010_0101)
    raw = encode_frame(MsgType.SET_LOADS, payload, seq=4)
    frame = decode_frame(raw)

    assert frame.msg_type == MsgType.SET_LOADS
    assert unpack_set_loads(frame.payload) == 0b1010_0101


def test_telemetry_payload_round_trip() -> None:
    payload = pack_telemetry(
        uptime_ms=12345,
        mode=Mode.SAFE,
        load_mask=0,
        fault_flags=FaultFlag.INJECTED,
        command_count=9,
        crc_error_count=2,
        last_command=MsgType.INJECT_FAULT,
        last_status=Status.OK,
    )

    telemetry = unpack_telemetry(payload)

    assert telemetry.uptime_ms == 12345
    assert telemetry.mode == Mode.SAFE
    assert telemetry.load_mask == 0
    assert telemetry.fault_flags == FaultFlag.INJECTED
    assert telemetry.command_count == 9
    assert telemetry.crc_error_count == 2
    assert telemetry.last_command == MsgType.INJECT_FAULT
    assert telemetry.last_status == Status.OK


def test_power_telemetry_payload_round_trip() -> None:
    payload = pack_power_telemetry(
        uptime_ms=12345,
        mode=Mode.NOMINAL,
        load_mask=0x03,
        sensor_status=PowerSensorStatus.PRESENT,
        bus_mV=3296,
        shunt_uV=430,
        current_mA_x10=43,
        power_mW=14,
    )

    telemetry = unpack_power_telemetry(payload)

    assert telemetry.uptime_ms == 12345
    assert telemetry.mode == Mode.NOMINAL
    assert telemetry.load_mask == 0x03
    assert telemetry.sensor_status == PowerSensorStatus.PRESENT
    assert telemetry.bus_mV == 3296
    assert telemetry.shunt_uV == 430
    assert telemetry.current_mA_x10 == 43
    assert telemetry.power_mW == 14


def test_ack_payload_round_trip() -> None:
    payload = pack_ack(MsgType.REQUEST_NOMINAL, Status.FAULT_ACTIVE, detail=1)
    ack = unpack_ack(payload)

    assert ack.request_type == MsgType.REQUEST_NOMINAL
    assert ack.status == Status.FAULT_ACTIVE
    assert ack.detail == 1


def test_fault_payload_round_trip() -> None:
    payload = pack_fault_flags(FaultFlag.INJECTED)
    assert unpack_fault_flags(payload) == FaultFlag.INJECTED


def test_decode_rejects_bad_crc() -> None:
    raw = bytearray(encode_frame(MsgType.GET_TELEMETRY, seq=1))
    raw[-1] ^= 0x01

    with pytest.raises(CrcError):
        decode_frame(bytes(raw))


def test_decode_rejects_truncated_frame() -> None:
    raw = encode_frame(MsgType.PING, seq=1)

    with pytest.raises(ProtocolError):
        decode_frame(raw[:-1])


def test_decode_rejects_oversized_payload() -> None:
    raw = bytearray(encode_frame(MsgType.PING, seq=1))
    raw[2 + 5] = 65

    with pytest.raises(ProtocolError):
        decode_frame(bytes(raw))


def test_unknown_message_type_decodes_for_firmware_to_nack() -> None:
    raw = encode_frame(0x55, seq=3)
    frame = decode_frame(raw)

    assert frame.msg_type == 0x55
    assert name_for_msg(frame.msg_type) == "UNKNOWN_0x55"


def test_packet_stream_recovers_from_noise_and_bad_crc() -> None:
    valid = encode_frame(MsgType.PING, seq=1)
    corrupt = bytearray(encode_frame(MsgType.GET_TELEMETRY, seq=2))
    corrupt[-2] ^= 0x7F
    later = encode_frame(MsgType.CLEAR_FAULTS, seq=3)

    stream = PacketStream()
    frames = stream.feed(b"\x00noise" + valid + bytes(corrupt) + later[:3])
    frames += stream.feed(later[3:])

    assert [frame.msg_type for frame in frames] == [MsgType.PING, MsgType.CLEAR_FAULTS]
    assert stream.crc_errors == 1
    assert stream.format_errors >= 1
