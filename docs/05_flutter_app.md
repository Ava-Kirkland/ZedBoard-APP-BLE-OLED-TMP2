# 04 — Flutter App

`Project: APP-BLE-OLED-TMP2 | Library: flutter_blue_plus ^2.3.10 | Flutter: 3.32.7 stable`

---

## Overview

The Flutter app connects a phone (Android or iOS) to the ZedBoard over BLE, sends `START_TEMP`, and displays live °C and °F readings at 1Hz. The entire app is a single file (`main.dart`) with a clean separation between BLE logic and UI rendering. V1 uses `setState` only — no external state management library.

---

## Architecture

Two layers, strictly separated:

**BLE Logic Layer** — Handles all Bluetooth operations: scanning, connecting, service discovery, subscribing, sending commands, and parsing data. Updates state variables and calls `setState()`. Has no knowledge of the UI.

**UI Layer** — Reads state variables and renders the appropriate screen. Never calls BLE APIs directly. Triggers BLE methods only via user tap handlers (Connect, Disconnect, Retry).

This means: restyle the UI without touching BLE code. Change BLE behavior without touching any widget code.

---

## BLE Connection Flow

> **Before connecting the app, the ZedBoard must be powered on with the Vitis application running and the Pmod BLE set up and advertising.** The power LED should be blinking — this confirms the RN4871 is advertising. See [ZedBoard-BLE](https://github.com/Ava-Kirkland/ZedBoard-BLE) for Pmod BLE setup and [02_vitis_setup.md](02_vitis_setup.md) for hardware setup and run instructions.

Every Connect tap runs this sequence. Each step must succeed before the next begins. Any failure goes to the error state.

### Step 1 — Request Permissions (Android only)

Request `BLUETOOTH_SCAN` and `BLUETOOTH_CONNECT` at runtime.

```dart
if (Platform.isAndroid) {
  // request permissions via permission_handler
}
```

> **Wrap in `Platform.isAndroid` unconditionally.** `permission_handler` returns `permanentlyDenied` for Android-specific Bluetooth keys on iOS, which causes the app to redirect iPhone users to Settings on every Connect tap. iOS Bluetooth permission is handled by the system prompt at first launch via `Info.plist` — the app never touches it.

### Step 2 — Scan

10-second BLE scan, filtered by device name (`RN4870` or `RN4871`). UUID filtering is not used — the RN4871 does not include its service UUID in its advertisement packet.

> **Race condition on retry:** `isScanning` stream emits `false` before the new scan registers. Fix: use a local `scanHasStarted` flag and call `stopScan()` before restarting.

### Step 3 — Connect

```dart
await device.connect(license: License.nonprofit);  // license param required in 2.3.10
```

15-second timeout. Register connection state listener here to watch for unexpected disconnects.

### Step 4 — Discover Services

Walk the GATT table looking for TX and RX characteristics by UUID. Both must be found or the flow fails.

### Step 5 — Subscribe to Notifications

```dart
await txChar.setNotifyValue(true);
```

Writes the CCCD on the device — tells the ZedBoard to push TX updates. Without this, no data arrives.

### Step 6 — Send START_TEMP

Write `START_TEMP\r\n` bytes to the RX characteristic. ZedBoard begins streaming at 1Hz.

---

## Incoming Data Pipeline

### Stage 1 — Buffer Assembly

BLE packets can be split across MTU boundaries. Accumulate bytes into a string buffer; drain complete `\r\n`-terminated lines in a `while` loop on each notification:

```dart
_buffer += String.fromCharCodes(value);
while (_buffer.contains('\r\n')) {
  final line = _buffer.substring(0, _buffer.indexOf('\r\n'));
  _buffer = _buffer.substring(_buffer.indexOf('\r\n') + 2);
  _handleLine(line);
}
```

### Stage 2 — Line Parsing (`_handleLine`)

```
TEMP:25.56C,78.01F   → parse tempC and tempF, update display, clear error
ERROR:SENSOR_FAIL    → show inline warning banner
ERROR:UNKNOWN_CMD    → show inline warning banner
%CONN_PARAM,...%     → silently ignored (RN4871 module event)
```

A valid `TEMP:` clears any previous error banner automatically.

---

## State Machine

Six states. The app is always in exactly one.

| State | Screen shown | Entered when |
|---|---|---|
| `disconnected` | Bluetooth-off icon, Connect button | App launch, clean disconnect, or reconnect failure |
| `scanning` | Spinner, "Scanning for ZedBoard…" | User taps Connect or Retry |
| `connecting` | Spinner, "Connecting to [name]…" | Device found during scan |
| `connected` | Brief transition, "starting stream…" | BLE handshake complete |
| `streaming` | Live °C (large), °F (small), green Live badge | START_TEMP sent successfully |
| `error` | Red icon, error message, Retry / Cancel | Any step in the flow fails |

### Transition Paths

```
Happy path:
disconnected → scanning → connecting → connected → streaming

Unexpected disconnect (reconnect succeeds):
streaming → disconnected → [2s delay] → connected → streaming

Unexpected disconnect (reconnect fails):
streaming → disconnected → error → (Retry) → scanning

User disconnect:
streaming → disconnected
```

---

## Disconnect Handling

**Unexpected disconnect** (connection state stream fires):
1. Wait 2 seconds
2. Attempt one automatic reconnect — skip scan, connect directly
3. If reconnect succeeds: re-run service discovery and re-send `START_TEMP` (BLE stack resets on disconnect — subscriptions do not persist)
4. If reconnect fails: go to error state with Retry button

**User disconnect** (Disconnect button):
1. Send `STOP_TEMP\r\n` to ZedBoard
2. Cancel notification subscription
3. Disconnect device
4. Reset all state to `disconnected`
5. No reconnect attempted

---

## Key Design Decisions

| Decision | Rationale |
|---|---|
| Scan by device name, not UUID | RN4871 does not advertise service UUID in payload |
| `License.nonprofit` in `connect()` | Required parameter in flutter_blue_plus 2.3.10 — not in older docs |
| Both °C and °F displayed simultaneously | ZedBoard sends both in every packet; no cost to showing both. C/F toggle is V2. |
| `setState` only, no Riverpod/Provider/Bloc | Single screen, V1 scope — external library would add overhead without benefit |
| Inline error banner during streaming | `ERROR:` packets show a warning without navigating away. A valid `TEMP:` clears it. Avoids disrupting live display for transient sensor errors. |
| One auto-reconnect, then manual Retry | Handles brief drops automatically; avoids infinite retry loops that drain battery |
| `Platform.isAndroid` guard on permissions | `permission_handler` returns wrong results for Android Bluetooth keys on iOS |

---

## Known Behaviors — Expected, Not Bugs

| Behavior | Reason |
|---|---|
| iOS requires delete + reinstall after first `pod install` | Native BLE code was not linked in the previous build — reinstall picks up the new native code |
| iOS requires Trust on first install | Sideloaded app (free Apple ID signing) — one-time per device |

---

## V2 Backlog

- C/F toggle
- App icon and display name
- ProGuard rule for Android release build
- Exponential backoff on auto-reconnect
