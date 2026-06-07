# Packet Protocol

Validation status: packet encode/decode and CRC behavior are covered by Python
tests and C++ host tests. Physical transports are validated only where recorded
in `bench_validation.md`.

## Frame Format

All frames are binary:

```text
sync[2] header[6] payload[N] crc16[2]
```

| Field | Size | Description |
| --- | ---: | --- |
| Sync | 2 | Fixed `A5 5A` |
| Version | 1 | Protocol version, currently `1` |
| Source | 1 | Sending node ID |
| Destination | 1 | Receiving node ID |
| Sequence | 1 | Host-selected transaction sequence |
| Message type | 1 | Command or response ID |
| Payload length | 1 | 0 to 64 bytes |
| Payload | N | Message-specific bytes |
| CRC-16 | 2 | Little-endian CRC-16/CCITT-FALSE over header and payload |

CRC does not include the sync bytes.

## Node IDs

| Name | Value |
| --- | ---: |
| `GROUND` | `0x01` |
| `EPS` | `0x02` |
| `OBC_BRIDGE` | `0x03` |
| `BROADCAST` | `0xFF` |

## Message IDs

| Name | Value | Payload |
| --- | ---: | --- |
| `PING` | `0x01` | none |
| `GET_TELEMETRY` | `0x02` | none |
| `SET_LOADS` | `0x03` | `uint8 load_mask` |
| `ENTER_SAFE` | `0x04` | none |
| `INJECT_FAULT` | `0x05` | `uint16 fault_flags` |
| `CLEAR_FAULTS` | `0x06` | none |
| `REQUEST_NOMINAL` | `0x07` | none |
| `GET_POWER_TELEMETRY` | `0x08` | none |
| `ACK` | `0x80` | `uint8 request_type, uint8 status, uint16 detail` |
| `NACK` | `0x81` | same as `ACK` |
| `TELEMETRY` | `0x82` | telemetry struct |
| `POWER_TELEMETRY` | `0x83` | INA219 power telemetry struct |

## Telemetry Payload

Little-endian layout:

| Field | Type | Description |
| --- | --- | --- |
| `uptime_ms` | `uint32` | Time since firmware boot |
| `mode` | `uint8` | `BOOT=0`, `NOMINAL=1`, `SAFE=2` |
| `load_mask` | `uint8` | Effective LED load state |
| `fault_flags` | `uint16` | Active fault flags |
| `command_count` | `uint16` | Valid command frames handled |
| `crc_error_count` | `uint16` | Bad CRC frames rejected |
| `last_command` | `uint8` | Last handled command type |
| `last_status` | `uint8` | Last command status |

## Power Telemetry Payload

Little-endian layout:

| Field | Type | Description |
| --- | --- | --- |
| `uptime_ms` | `uint32` | Time since firmware boot |
| `mode` | `uint8` | `BOOT=0`, `NOMINAL=1`, `SAFE=2` |
| `load_mask` | `uint8` | Effective LED load state |
| `sensor_status` | `uint8` | INA219 status bits |
| `bus_mV` | `uint16` | INA219 bus voltage in millivolts |
| `shunt_uV` | `int32` | INA219 shunt voltage in microvolts |
| `current_mA_x10` | `int16` | Current in tenths of a milliamp |
| `power_mW` | `uint16` | Power in milliwatts |

Power sensor status bits:

| Name | Value | Meaning |
| --- | ---: | --- |
| `PRESENT` | `0x01` | INA219 read succeeded |
| `I2C_ERROR` | `0x02` | INA219 read failed |
| `MATH_OVERFLOW` | `0x04` | INA219 overflow flag was set |

## Fault Flags

| Name | Value | Meaning |
| --- | ---: | --- |
| `INJECTED` | `0x0001` | Operator-injected demo fault |
| `BAD_COMMAND` | `0x0002` | Reserved for future diagnostics |

## Status Codes

| Name | Value |
| --- | ---: |
| `OK` | `0` |
| `BAD_LENGTH` | `1` |
| `BAD_STATE` | `2` |
| `UNKNOWN_COMMAND` | `3` |
| `FAULT_ACTIVE` | `4` |
| `CRC_ERROR` | `5` |

## Safety Semantics

- A bad CRC frame is ignored and does not receive `ACK` or `NACK`.
- `SET_LOADS` is rejected in `SAFE`.
- `CLEAR_FAULTS` does not leave `SAFE`.
- `REQUEST_NOMINAL` succeeds only when `fault_flags == 0`.
