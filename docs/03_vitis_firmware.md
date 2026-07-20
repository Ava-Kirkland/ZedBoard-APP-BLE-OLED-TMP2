# 02 — Vitis Firmware

`Project: BLE-OLED-TMP2 | Tools: Vitis 2025.2 | Language: Bare-metal C`

---

## Overview

`main.c` implements a bare-metal firmware loop on the Zynq PS ARM core. It samples temperature once per second using a non-blocking timer, displays it on the OLED, and streams it over BLE when the app has issued `START_TEMP`. It also handles RN4871 module events (`%STREAM_OPEN%`, `%DISCONNECT%`) and CMD mode passthrough for direct RN4871 configuration.

---

## Key Concepts

**Non-blocking timer:** Uses `XTime_GetTime()` from `xiltimer.h` to check elapsed time without calling `sleep()`. The main loop runs continuously — peripherals are serviced every pass, and the 1Hz temperature sample fires only when enough time has elapsed.

**`TempParts` struct:** Temperature is decomposed into `{sign, whole, frac}` exactly once per cycle. OLED, Tera Term, and BLE all format from the same struct. This eliminates rounding divergence between outputs.

**Dual terminator buffer:** The UART0 receive buffer watches for two terminators: `\r\n` (normal command/response end) and `%...%` (RN4871 module events). Required because RN4871 sends `%STREAM_OPEN%` and `%CONN_PARAM,...%` back-to-back with no `\r\n` between them. Using `\r\n`-only would drop the second event.

**CMD mode:** When `$$$` is detected on UART1, the firmware enters CMD mode — all UART0 output stops, and UART1 input is forwarded to UART0 (with `\n` suppressed). `---` on UART1 exits CMD mode. This allows direct RN4871 configuration over Tera Term without interference from the firmware's normal output.

---

## State Variables

```c
bool streaming = false;   // app has sent START_TEMP; firmware pushes TEMP: packets
bool connected = false;   // %STREAM_OPEN% received from RN4871
bool cmd_mode  = false;   // $$$ detected on UART1; gates all UART0 output

int dollar_count = 0;     // consecutive $ chars on UART1 (entry detection)
int dash_count   = 0;     // consecutive - chars on UART1 (exit detection)
```

---

## Main Loop Structure

```
loop:
  ├── Check UART1 (Tera Term) for CMD mode entry/exit ($$$, ---)
  ├── If cmd_mode: forward UART1 bytes to UART0 (suppress \n), skip everything else
  ├── Check UART0 (BLE) for incoming bytes → append to buffer
  │     ├── On \r\n: pass line to parse_command()
  │     └── On %...%: pass event to handle_ble_event()
  ├── If timer elapsed (1Hz):
  │     ├── Read ADT7420 over I2C
  │     ├── If read fails: send ERROR:SENSOR_FAIL\r\n (if streaming), skip decompose
  │     ├── Compute TempParts once
  │     ├── Update onboard OLED via custom AXI-Lite IP driver
  │     ├── xil_printf to Tera Term
  │     └── If streaming && !cmd_mode: send TEMP:...\r\n over UART0
  └── repeat
```

---

## `parse_command` Signature

```c
void parse_command(char *buf, bool *connected, bool *streaming);
```

Handles:
- `START_TEMP\r\n` → sets `*streaming = true`
- `STOP_TEMP\r\n` → sets `*streaming = false`
- Any non-`%` unrecognized input → sends `ERROR:UNKNOWN_CMD\r\n`

CMD mode detection and exit are handled entirely on the UART1 path — `parse_command` never sees `$$$` or `---`.

---

## Timer API — Vitis 2025.2

> **Vitis 2025.2 uses `xiltimer.h`, not the legacy `xtime_l.h`.**

```c
#include "xiltimer.h"

XTime now;
XTime_GetTime(&now);

// Elapsed time check
if ((now - last_sample) >= COUNTS_PER_SECOND) {
    last_sample = now;
    // ... sample temperature
}
```

`COUNTS_PER_SECOND` is defined in `xtimer_config.h` as `XPAR_CPU_CORE_CLOCK_FREQ_HZ / 2`.  
See [tool_version_differences.md](tool_version_differences.md) for the xtime_l vs xiltimer difference.

---

## RN4871 Event Handling

The RN4871 sends module status events enclosed in `%`: `%STREAM_OPEN%`, `%DISCONNECT%`, `%CONN_PARAM,...%`.

The firmware detects these using `strstr` (not `strcmp`) because events can arrive concatenated:
```
%STREAM_OPEN%%CONN_PARAM,0006,0000,0002,0C80%
```

`strcmp` would miss the second event. `strstr` finds both.

> **`%STREAM_OPEN%` is ZedBoard-internal.** It never appears in the BLE data stream the phone app sees. The RN4871 sends it to UART0 to notify the firmware a BLE connection is active.

---

## CMD Mode Detail

CMD mode allows direct RN4871 configuration (e.g., changing device name, checking firmware version) over Tera Term without the firmware interfering.

**Entry:** Type `$$$` in Tera Term → firmware detects three consecutive `$` on UART1 → sets `cmd_mode = true` → stops all UART0 output → forwards subsequent UART1 input to UART0.

**Exit:** Type `---` in Tera Term → firmware detects three consecutive `-` on UART1 → sets `cmd_mode = false` → resets `last_sample` via `XTime_GetTime()` to avoid a stale timer → resumes normal operation.

**Why `\n` is suppressed in CMD forward path:**  
The RN4871 uses `\r` as its command terminator. Forwarding raw `\r\n` from Tera Term sends a bare `\n` after each command, which the RN4871 interprets as an empty command and responds with `Err`. The firmware strips `\n` and forwards only `\r`.

---

## Sensor Error Handling

If the ADT7420 I2C read returns an error sentinel:

```c
if (temp_raw == ERROR_SENTINEL) {
    if (streaming) {
        send_uart0("ERROR:SENSOR_FAIL\r\n");
    }
    // skip TempParts decomposition, OLED update, BLE push
} else {
    // normal path: decompose, display, stream
}
```

Using `if/else` is required — a fallthrough would pass a garbage value to `DecomposeTemp`.

---

## Debug Notes

- Bug #1: `xil_printf` routing to BLE → see [bugs_and_fixes.md](bugs_and_fixes.md#bug-1--xilprintf-output-routed-to-ble-instead-of-tera-term)
- Bug #2: OLED and BLE showing 0.01°F difference → see [bugs_and_fixes.md](bugs_and_fixes.md#bug-2--oled-and-ble-temperature-differ-by-001f)
- Bug #3: `Err` spam in CMD mode → see [bugs_and_fixes.md](bugs_and_fixes.md#bug-3--err-spam-from-rn4871-during-cmd-mode)
- Bug #4: CMD mode never exiting → see [bugs_and_fixes.md](bugs_and_fixes.md#bug-4--cmd-mode-never-exits)
- Bug #5: Sensor error falling through to `DecomposeTemp` → see [bugs_and_fixes.md](bugs_and_fixes.md#bug-5--sensor-error-falls-through-to-decomposetemp)

---

## Potential Improvements

- **Interrupt-driven UART:** Current implementation polls UART0 every loop iteration. Interrupt-driven receive with a ring buffer would be cleaner and allow the main loop to sleep between samples. Deferred — polling is sufficient for 1Hz streaming.
- **I2C error retry:** Current behavior on `SENSOR_FAIL` is to skip the cycle. A retry with backoff would be more robust for transient I2C glitches.
