#!/usr/bin/env python3
"""Ground-station CLI for the PicoSat EPS command/telemetry demo."""

from __future__ import annotations

import argparse
import csv
import pathlib
import sys
import time

from protocol import (
    CrcError,
    FaultFlag,
    Frame,
    MsgType,
    NodeId,
    PacketStream,
    Status,
    decode_frame,
    encode_frame,
    name_for_mode,
    name_for_msg,
    name_for_status,
    pack_fault_flags,
    pack_set_loads,
    unpack_ack,
    unpack_power_telemetry,
    unpack_telemetry,
)


DEFAULT_BAUD = 115200


def import_serial():
    try:
        import serial  # type: ignore
    except ModuleNotFoundError as exc:
        raise SystemExit("pyserial is required: python3 -m pip install pyserial") from exc
    return serial


def hex_bytes(data: bytes) -> str:
    return " ".join(f"{byte:02X}" for byte in data)


def build_command(args: argparse.Namespace) -> tuple[MsgType, bytes]:
    if args.command == "ping":
        return MsgType.PING, b""
    if args.command == "telemetry":
        return MsgType.GET_TELEMETRY, b""
    if args.command == "set-loads":
        return MsgType.SET_LOADS, pack_set_loads(args.mask)
    if args.command == "enter-safe":
        return MsgType.ENTER_SAFE, b""
    if args.command == "inject-fault":
        return MsgType.INJECT_FAULT, pack_fault_flags(args.flags)
    if args.command == "clear-faults":
        return MsgType.CLEAR_FAULTS, b""
    if args.command == "request-nominal":
        return MsgType.REQUEST_NOMINAL, b""
    if args.command == "power":
        return MsgType.GET_POWER_TELEMETRY, b""
    if args.command == "bad-crc":
        return MsgType.PING, b""
    raise SystemExit(f"unknown command: {args.command}")


def print_frame(frame: Frame) -> None:
    print(
        f"rx seq={frame.seq} src=0x{frame.src:02X} dst=0x{frame.dst:02X} "
        f"type={name_for_msg(frame.msg_type)} payload={hex_bytes(frame.payload)}"
    )
    if frame.msg_type in (MsgType.ACK, MsgType.NACK):
        ack = unpack_ack(frame.payload)
        print(
            f"  {name_for_msg(frame.msg_type)} request={name_for_msg(ack.request_type)} "
            f"status={name_for_status(ack.status)} detail={ack.detail}"
        )
    elif frame.msg_type == MsgType.TELEMETRY:
        telemetry = unpack_telemetry(frame.payload)
        print(
            "  TELEMETRY "
            f"uptime_ms={telemetry.uptime_ms} "
            f"mode={name_for_mode(telemetry.mode)} "
            f"load_mask=0x{telemetry.load_mask:02X} "
            f"fault_flags=0x{telemetry.fault_flags:04X} "
            f"command_count={telemetry.command_count} "
            f"crc_error_count={telemetry.crc_error_count} "
            f"last_command={name_for_msg(telemetry.last_command)} "
            f"last_status={name_for_status(telemetry.last_status)}"
        )
    elif frame.msg_type == MsgType.POWER_TELEMETRY:
        telemetry = unpack_power_telemetry(frame.payload)
        status_bits = []
        if telemetry.sensor_status & 0x01:
            status_bits.append("PRESENT")
        if telemetry.sensor_status & 0x02:
            status_bits.append("I2C_ERROR")
        if telemetry.sensor_status & 0x04:
            status_bits.append("MATH_OVERFLOW")
        if not status_bits:
            status_bits.append("NOT_PRESENT")
        print(
            "  POWER "
            f"uptime_ms={telemetry.uptime_ms} "
            f"mode={name_for_mode(telemetry.mode)} "
            f"load_mask=0x{telemetry.load_mask:02X} "
            f"sensor_status=0x{telemetry.sensor_status:02X}({','.join(status_bits)}) "
            f"bus_mV={telemetry.bus_mV} "
            f"shunt_uV={telemetry.shunt_uV} "
            f"current_mA={telemetry.current_mA_x10 / 10:.1f} "
            f"power_mW={telemetry.power_mW}"
        )


def append_telemetry_log(path: pathlib.Path, frame: Frame) -> None:
    if frame.msg_type != MsgType.TELEMETRY:
        return
    telemetry = unpack_telemetry(frame.payload)
    exists = path.exists()
    with path.open("a", newline="", encoding="utf-8") as file:
        writer = csv.writer(file)
        if not exists:
            writer.writerow(
                [
                    "host_time_s",
                    "uptime_ms",
                    "mode",
                    "load_mask",
                    "fault_flags",
                    "command_count",
                    "crc_error_count",
                    "last_command",
                    "last_status",
                ]
            )
        writer.writerow(
            [
                f"{time.time():.3f}",
                telemetry.uptime_ms,
                name_for_mode(telemetry.mode),
                f"0x{telemetry.load_mask:02X}",
                f"0x{telemetry.fault_flags:04X}",
                telemetry.command_count,
                telemetry.crc_error_count,
                name_for_msg(telemetry.last_command),
                name_for_status(telemetry.last_status),
            ]
        )


def read_raw_response(port, timeout_s: float) -> list[Frame]:
    stream = PacketStream()
    deadline = time.monotonic() + timeout_s
    frames: list[Frame] = []

    while time.monotonic() < deadline:
        chunk = port.read(64)
        if chunk:
            frames.extend(stream.feed(chunk))
            if frames:
                return frames
        else:
            time.sleep(0.01)
    return frames


def read_hex_response(port, timeout_s: float) -> list[Frame]:
    deadline = time.monotonic() + timeout_s
    buffer = bytearray()

    while time.monotonic() < deadline:
        chunk = port.read(64)
        if not chunk:
            time.sleep(0.01)
            continue

        buffer.extend(chunk)
        while b"\n" in buffer:
            line, _, rest = buffer.partition(b"\n")
            buffer = bytearray(rest)
            line = line.strip()
            if not line:
                continue
            try:
                frame = decode_frame(bytes.fromhex(line.decode("ascii")))
            except (UnicodeDecodeError, ValueError, CrcError):
                continue
            if frame.src == NodeId.EPS and frame.dst == NodeId.GROUND:
                return [frame]
            print(f"ignored echo/noise frame: {name_for_msg(frame.msg_type)}")
    return []


def make_wire_bytes(
    args: argparse.Namespace,
    msg_type: MsgType,
    payload: bytes,
    corrupt_crc: bool = False,
) -> tuple[bytes, bytes]:
    raw = bytearray(
        encode_frame(msg_type, payload, seq=args.seq, src=NodeId.GROUND, dst=NodeId.EPS)
    )
    if corrupt_crc:
        raw[-1] ^= 0x01

    wire_bytes = bytes(raw)
    if args.transport == "hex":
        wire_bytes = bytes(raw).hex().upper().encode("ascii") + b"\n"
    return bytes(raw), wire_bytes


def print_tx(args: argparse.Namespace, msg_type: MsgType, raw: bytes, wire_bytes: bytes) -> None:
    if args.transport == "hex":
        print(f"tx {name_for_msg(msg_type)} seq={args.seq} hex-line: {wire_bytes.decode().strip()}")
    else:
        print(f"tx {name_for_msg(msg_type)} seq={args.seq}: {hex_bytes(raw)}")


def send_command_on_open_port(
    port,
    args: argparse.Namespace,
    msg_type: MsgType,
    payload: bytes,
    corrupt_crc: bool = False,
) -> list[Frame]:
    raw, wire_bytes = make_wire_bytes(args, msg_type, payload, corrupt_crc)
    print_tx(args, msg_type, raw, wire_bytes)
    port.write(wire_bytes)
    port.flush()
    if args.transport == "hex":
        return read_hex_response(port, args.timeout)
    return read_raw_response(port, args.timeout)


def transact(args: argparse.Namespace, msg_type: MsgType, payload: bytes, corrupt_crc: bool = False) -> list[Frame]:
    raw, wire_bytes = make_wire_bytes(args, msg_type, payload, corrupt_crc)

    if args.dry_run:
        print_tx(args, msg_type, raw, wire_bytes)
        if corrupt_crc:
            print("dry-run only: CRC byte was deliberately corrupted")
        return []

    serial = import_serial()
    try:
        with serial.Serial(args.port, args.baud, timeout=0.05) as port:
            settle = max(args.settle, 1.0) if args.transport == "hex" else args.settle
            time.sleep(settle)
            port.reset_input_buffer()
            return send_command_on_open_port(port, args, msg_type, payload, corrupt_crc)
    except serial.SerialException as exc:
        raise SystemExit(f"serial error on {args.port}: {exc}") from exc


def demo_steps() -> list[tuple[str, MsgType, bytes, bool, bool]]:
    return [
        ("link check", MsgType.PING, b"", False, True),
        ("initial telemetry", MsgType.GET_TELEMETRY, b"", False, True),
        ("turn on load 0", MsgType.SET_LOADS, pack_set_loads(0x01), False, True),
        ("turn on loads 0 and 1", MsgType.SET_LOADS, pack_set_loads(0x03), False, True),
        ("telemetry after load command", MsgType.GET_TELEMETRY, b"", False, True),
        ("inject fault and enter SAFE", MsgType.INJECT_FAULT, pack_fault_flags(FaultFlag.INJECTED), False, True),
        ("attempt load command while SAFE", MsgType.SET_LOADS, pack_set_loads(0x0F), False, True),
        ("telemetry in SAFE", MsgType.GET_TELEMETRY, b"", False, True),
        ("request NOMINAL while fault active", MsgType.REQUEST_NOMINAL, b"", False, True),
        ("clear fault flags", MsgType.CLEAR_FAULTS, b"", False, True),
        ("request NOMINAL after clearing faults", MsgType.REQUEST_NOMINAL, b"", False, True),
        ("turn on load 0 after recovery", MsgType.SET_LOADS, pack_set_loads(0x01), False, True),
        ("send deliberately corrupted CRC", MsgType.PING, b"", True, False),
        ("telemetry after bad CRC", MsgType.GET_TELEMETRY, b"", False, True),
    ]


def power_demo_steps() -> list[tuple[str, MsgType, bytes, bool, bool]]:
    return [
        ("link check", MsgType.PING, b"", False, True),
        ("clear any old fault flags", MsgType.CLEAR_FAULTS, b"", False, True),
        ("request NOMINAL", MsgType.REQUEST_NOMINAL, b"", False, True),
        ("command all measured loads off", MsgType.SET_LOADS, pack_set_loads(0x00), False, True),
        ("power telemetry with loads off", MsgType.GET_POWER_TELEMETRY, b"", False, True),
        ("turn on measured load 0", MsgType.SET_LOADS, pack_set_loads(0x01), False, True),
        ("power telemetry with load 0 on", MsgType.GET_POWER_TELEMETRY, b"", False, True),
        ("turn on measured loads 0 and 1", MsgType.SET_LOADS, pack_set_loads(0x03), False, True),
        ("power telemetry with loads 0 and 1 on", MsgType.GET_POWER_TELEMETRY, b"", False, True),
        ("inject fault and enter SAFE", MsgType.INJECT_FAULT, pack_fault_flags(FaultFlag.INJECTED), False, True),
        ("power telemetry in SAFE", MsgType.GET_POWER_TELEMETRY, b"", False, True),
        ("clear fault flags", MsgType.CLEAR_FAULTS, b"", False, True),
        ("request NOMINAL after clearing faults", MsgType.REQUEST_NOMINAL, b"", False, True),
        ("turn on measured load 0 after recovery", MsgType.SET_LOADS, pack_set_loads(0x01), False, True),
        ("power telemetry after recovery", MsgType.GET_POWER_TELEMETRY, b"", False, True),
    ]


def run_scripted_steps(
    args: argparse.Namespace,
    steps: list[tuple[str, MsgType, bytes, bool, bool]],
) -> int:
    if args.dry_run:
        for index, (label, msg_type, payload, corrupt_crc, _expect_response) in enumerate(steps, 1):
            args.seq = index
            print(f"\n== {index}. {label} ==")
            raw, wire_bytes = make_wire_bytes(args, msg_type, payload, corrupt_crc)
            print_tx(args, msg_type, raw, wire_bytes)
        return 0

    serial = import_serial()
    try:
        with serial.Serial(args.port, args.baud, timeout=0.05) as port:
            settle = max(args.settle, 1.0) if args.transport == "hex" else args.settle
            print(f"opened {args.port}; waiting {settle:.1f}s for firmware")
            time.sleep(settle)
            port.reset_input_buffer()

            for index, (label, msg_type, payload, corrupt_crc, expect_response) in enumerate(steps, 1):
                args.seq = index
                print(f"\n== {index}. {label} ==")
                frames = send_command_on_open_port(port, args, msg_type, payload, corrupt_crc)
                if not frames and expect_response:
                    print("no response before timeout")
                    return 1
                if not frames and not expect_response:
                    print("no response to corrupted frame, as expected")
                    continue
                if frames and not expect_response:
                    print("unexpected response to corrupted frame")
                    for frame in frames:
                        print_frame(frame)
                    return 1
                for frame in frames:
                    print_frame(frame)
                    if args.log:
                        append_telemetry_log(pathlib.Path(args.log), frame)
                time.sleep(0.2)
    except serial.SerialException as exc:
        raise SystemExit(f"serial error on {args.port}: {exc}") from exc

    return 0


def run_demo(args: argparse.Namespace) -> int:
    return run_scripted_steps(args, demo_steps())


def run_power_demo(args: argparse.Namespace) -> int:
    return run_scripted_steps(args, power_demo_steps())


def run(args: argparse.Namespace) -> int:
    if args.command == "demo":
        return run_demo(args)
    if args.command == "power-demo":
        return run_power_demo(args)

    msg_type, payload = build_command(args)

    if args.command == "bad-crc":
        frames = transact(args, msg_type, payload, corrupt_crc=True)
        if args.dry_run:
            return 0
        if frames:
            print("unexpected response to corrupted frame:")
            for frame in frames:
                print_frame(frame)
            return 1
        print("no response to corrupted frame, as expected")
        return 0

    frames = transact(args, msg_type, payload)
    if args.dry_run:
        return 0
    if not frames:
        print("no response before timeout")
        return 1

    for frame in frames:
        print_frame(frame)
        if args.log:
            append_telemetry_log(pathlib.Path(args.log), frame)
    return 0


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="/dev/ttyACM0", help="USB serial device")
    parser.add_argument("--baud", type=int, default=DEFAULT_BAUD, help="serial baud rate")
    parser.add_argument("--timeout", type=float, default=1.0, help="response timeout in seconds")
    parser.add_argument("--settle", type=float, default=0.1, help="delay after opening serial")
    parser.add_argument("--seq", type=int, default=1, help="packet sequence number")
    parser.add_argument(
        "--transport",
        choices=("raw", "hex"),
        default="raw",
        help="raw binary for C++ firmware, hex for MicroPython fallback",
    )
    parser.add_argument("--dry-run", action="store_true", help="print encoded packet without serial I/O")
    parser.add_argument("--log", help="append telemetry responses to CSV")

    subparsers = parser.add_subparsers(dest="command", required=True)
    subparsers.add_parser("ping")
    subparsers.add_parser("telemetry")

    set_loads = subparsers.add_parser("set-loads")
    set_loads.add_argument("mask", type=lambda value: int(value, 0), help="load mask, e.g. 0x03")

    subparsers.add_parser("enter-safe")

    inject_fault = subparsers.add_parser("inject-fault")
    inject_fault.add_argument(
        "--flags",
        type=lambda value: int(value, 0),
        default=FaultFlag.INJECTED,
        help="fault flags to set, default injected fault",
    )

    subparsers.add_parser("clear-faults")
    subparsers.add_parser("request-nominal")
    subparsers.add_parser("power")
    subparsers.add_parser("bad-crc")
    subparsers.add_parser("demo")
    subparsers.add_parser("power-demo")

    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)
    try:
        return run(args)
    except CrcError as exc:
        print(f"crc error in response: {exc}")
        return 1
    except KeyboardInterrupt:
        print("interrupted")
        return 130


if __name__ == "__main__":
    raise SystemExit(main())
