# 03 — BLE Protocol

`Project: BLE-OLED-TMP2 | Hardware: Pmod BLE (RN4871) | Service: Transparent UART`

---

## Overview

The RN4871 runs Microchip's Transparent UART Service — a standard BLE profile that turns a BLE connection into a bidirectional serial channel. The phone app and ZedBoard firmware communicate using a simple ASCII text protocol over this channel. Both sides must agree on the format; if the firmware packet format changes, only `_handleLine()` in `main.dart` needs updating.

---

## Key Concepts

**Transparent UART Service:** A BLE GATT service provided by the RN4871 that exposes two characteristics — one for sending data to the phone (TX), one for receiving commands from the phone (RX). The phone subscribes to TX notifications; the ZedBoard writes to RX.

**GATT (Generic Attribute Profile):** The BLE protocol layer that defines services and characteristics. Think of it as a structured API the peripheral exposes over BLE.

**TX/RX naming convention:** From the hardware's perspective. TX = ZedBoard sends, phone reads. RX = phone writes, ZedBoard receives. In `flutter_blue_plus`, the TX characteristic is the one you call `setNotifyValue(true)` on; the RX characteristic is the one you write to.

---

## UUIDs

```
Service:         49535343-FE7D-4AE5-8FA9-9FAFD205E455
TX Characteristic (ZedBoard → Phone, notify):
                 49535343-1E4D-4BD9-BA61-23C647249616
RX Characteristic (Phone → ZedBoard, write):
                 49535343-8841-43F4-A8D4-ECBE34729BB3
```

These are fixed Microchip-defined UUIDs for the Transparent UART Service. Do not change them.

---

## Command Table

| Message | Direction | Effect |
|---|---|---|
| `START_TEMP\r\n` | Phone → ZedBoard (RX) | Firmware sets `streaming = true`; begins pushing `TEMP:` packets at 1Hz |
| `STOP_TEMP\r\n` | Phone → ZedBoard (RX) | Firmware sets `streaming = false`; stops pushing |
| `TEMP:23.56C,74.41F\r\n` | ZedBoard → Phone (TX) | Live temperature reading, both units |
| `ERROR:SENSOR_FAIL\r\n` | ZedBoard → Phone (TX) | ADT7420 I2C read failed |
| `ERROR:UNKNOWN_CMD\r\n` | ZedBoard → Phone (TX) | Firmware received an unrecognized command |

---

## Packet Format

```
TEMP:23.56C,74.41F\r\n        ← normal positive reading
TEMP:-10.00C,14.00F\r\n       ← negative temperature
ERROR:SENSOR_FAIL\r\n
ERROR:UNKNOWN_CMD\r\n
```

- All packets are ASCII text
- Terminated with `\r\n` (bytes `0x0D 0x0A`) — both bytes required
- ZedBoard always sends both °C and °F in every `TEMP:` packet
- The phone buffers incoming bytes and processes only complete `\r\n`-terminated lines

---

## Scan Strategy

The RN4871 advertises as `RN4870-7F97` (device name; the suffix is hardware-specific).

**The RN4871 does not include its service UUID in its advertisement payload.** This means UUID-based BLE scan filtering will not find it. The Flutter app filters by device name instead:

```dart
advName.contains('RN4870') || advName.contains('RN4871')
```

A service UUID fallback is included for iOS compatibility. For future projects using different BLE hardware, prefer UUID-based filtering — it's more reliable and not name-dependent.

---

## Protocol Notes

- `%STREAM_OPEN%` and `%DISCONNECT%` are RN4871 module events sent to the ZedBoard over UART0. They are **not** part of the BLE data stream the phone sees.
- `START_TEMP` must be sent explicitly after connecting — the ZedBoard does not auto-start streaming on BLE connection.
- BLE subscriptions and streaming state do not persist across a disconnect. On reconnect, the app must re-run service discovery and re-send `START_TEMP`.
- CMD mode on the ZedBoard causes a gap in `TEMP:` packets. The app handles this via reconnect timeout/retry — no special protocol handling needed.

---

## Why Transparent UART (Not Custom GATT)

Transparent UART was chosen because:
- The RN4871 supports it out of the box — no custom GATT configuration required
- It behaves like a serial port, which maps cleanly to the ZedBoard's UART-based firmware
- The protocol complexity lives in the ASCII text layer, not the BLE layer — easier to debug with nRF Connect

The tradeoff: no per-characteristic semantics. Everything is a string on one channel. For V1 scope this is the right call.
