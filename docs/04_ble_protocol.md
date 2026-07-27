# 04 — BLE Protocol

`Project: APP-BLE-OLED-TMP2 | Module: RN4871 Pmod BLE | Service: Transparent UART`

---

## Overview

The ZedBoard communicates with the Flutter app over BLE using the RN4871's Transparent UART Service. The protocol is a simple command/response model layered on top of raw UART bytes. The ZedBoard firmware accumulates 5 valid temperature readings (sampled every ~200ms, non-blocking), computes the average, and pushes a formatted packet to the RN4871 once per approximately 1-second cycle. The phone app sends `START_TEMP` to begin streaming and `STOP_TEMP` to end it.

---

## BLE UUIDs — RN4871 Transparent UART Service

| Role | UUID |
|------|------|
| Service | `49535343-FE7D-4AE5-8FA9-9FAFD205E455` |
| TX Characteristic (ZedBoard → Phone) | `49535343-1E4D-4BD9-BA61-23C647249616` |
| RX Characteristic (Phone → ZedBoard) | `49535343-8841-43F4-A8D4-ECBE34729BB3` |

> **The RN4871 does not advertise its service UUID in the advertisement payload.** Scan by device name (`RN4870` or `RN4871`). UUID-based filtering will not find the device.

---

## Commands — Phone → ZedBoard

| Command | Bytes (hex) | Effect |
|---------|-------------|--------|
| `START_TEMP\r\n` | `53 54 41 52 54 5F 54 45 4D 50 0D 0A` | Begin temperature streaming |
| `STOP_TEMP\r\n` | `53 54 4F 50 5F 54 45 4D 50 0D 0A` | Stop temperature streaming |

- `START_TEMP` only takes effect if the firmware's `connected` flag is true (i.e., `%STREAM_OPEN%` has been received from the RN4871)
- `STOP_TEMP` always stops streaming regardless of connection state

---

## Packets — ZedBoard → Phone

### Temperature packet (normal)

```
TEMP:<sign><whole>.<frac>C,<sign><whole>.<frac>F\r\n
```

Example:
```
TEMP:+23.56C,+74.41F\r\n
```

- Sign is `+` or `-`
- Celsius and Fahrenheit are both computed from the same averaged reading, decomposed once into `TempParts` structs — no rounding divergence between the two values
- Sent approximately once per second (after 5 valid samples at ~200ms intervals)
- Only sent when `streaming == true` and `cmd_mode == false`

### Error packet

```
ERROR:SENSOR_FAIL\r\n
ERROR:UNKNOWN_CMD\r\n
```

`SENSOR_FAIL` is sent if the averaged temperature value is at or below `ADT7420_SENTINEL_THRESHOLD` (should not occur in practice since bad reads are skipped during accumulation, but guards the output path).  
`UNKNOWN_CMD` is sent if an unrecognized command is received on UART0.

---

## RN4871 Module Events (UART0, ZedBoard-internal)

The RN4871 sends status events to UART0 that the firmware must handle. These are **never forwarded to the phone app** — they are consumed by the firmware.

| Event | Trigger | Firmware action |
|-------|---------|-----------------|
| `%STREAM_OPEN%` | Phone app connects and subscribes | `connected = true`; if `oled_on == false`: call `OLED_RepowerOn()`, set `oled_on = true` |
| `%DISCONNECT%` | Phone disconnects | `connected = false`, `streaming = false`; call `OLED_Off()`, set `oled_on = false` |
| `%CONN_PARAM,...%` | Connection parameters negotiated | Received and parsed (no firmware action) |

> These events arrive on UART0 without `\r\n` terminators, and can be concatenated (e.g., `%STREAM_OPEN%%CONN_PARAM,...%`). The firmware uses `strstr` for matching, not `strcmp`, to handle concatenation.

---

## Connection State Machine

```
[Boot]
  │
  ▼
oled_on=true, connected=false, streaming=false
  │
  │  %STREAM_OPEN%
  ▼
connected=true
If oled was off: OLED_RepowerOn(), oled_on=true
  │
  │  START_TEMP\r\n (from phone)
  ▼
streaming=true → TEMP packets sent every ~1 second
  │
  │  STOP_TEMP\r\n  ──────────────────────────────────► streaming=false
  │
  │  %DISCONNECT%
  ▼
connected=false, streaming=false
OLED_Off(), oled_on=false
  │
  │  %STREAM_OPEN% (phone reconnects)
  └─────────────────────────────────────────────────► (back to connected state)
```

---

## Scanning Notes (Flutter App)

- Scan filter: device name contains `RN4870` or `RN4871` (the module advertises as `RN4870-XXXX` by default)
- Service UUID fallback available for iOS if name filter is insufficient
- The RN4871 does not include its Transparent UART service UUID in the advertisement payload — UUID-based scan filters will not work

---

## nRF Connect Verification

To verify firmware independently of the Flutter app:

1. Connect nRF Connect to the RN4871
2. Find the Transparent UART TX characteristic (`49535343-1E4D-4BD9-BA61-23C647249616`) and enable notifications
3. Write `START_TEMP\r\n` (hex: `53 54 41 52 54 5F 54 45 4D 50 0D 0A`) to the RX characteristic (`49535343-8841-43F4-A8D4-ECBE34729BB3`)
4. Notifications should begin arriving approximately once per second with `TEMP:+XX.XXC,+XX.XXF`