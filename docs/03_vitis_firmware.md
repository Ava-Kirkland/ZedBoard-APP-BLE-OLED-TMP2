# 03 — Vitis Firmware

`Project: APP-BLE-OLED-TMP2 | Tools: Vitis 2025.2 | Language: Bare-metal C`

---

## Overview

The firmware runs on the Zynq PS ARM core as a bare-metal application. It samples temperature from the ADT7420 five times per second using non-blocking timer polling, accumulates valid readings, and sends the average to all outputs once 5 valid samples are collected. It displays temperature on the OLED (when on), streams it over BLE when the app has issued `START_TEMP`, and prints to Tera Term continuously. It also handles RN4871 module events (`%STREAM_OPEN%`, `%DISCONNECT%`) — including powering the OLED off on disconnect and back on when the phone reconnects — and CMD mode passthrough for direct RN4871 configuration.

---

## Source File Structure

| File | Role |
|------|------|
| `main.c` | Top-level loop — sampling, averaging, UART parsing, OLED toggle dispatch |
| `adt7420.c/.h` | ADT7420 I2C driver — init, read, decompose, print |
| `oled.c/.h` | OLED AXI driver — init, write, clear, power off (`OLED_Off`), power on (`OLED_RepowerOn`) |
| `ble_uart.c/.h` | UART init — UART0 (BLE, `ps7_uart_0`) and UART1 (Tera Term, `ps7_uart_1`) |
| `temp_display.c/.h` | Integration layer — formats `TempParts` structs and writes 4-line layout to OLED |

Copy all five `.c` and `.h` files into `src/` alongside `main.c` in the Vitis application. The Vitis 2025.x TCL driver loading mechanism is broken for custom IP — manual copy is required.

---

## Key Concepts

**Non-blocking timer with sub-second sampling:** Uses `XTime_GetTime()` from `xiltimer.h`. The timer fires every `COUNTS_PER_SECOND / NUM_SAMPLES` ticks (200ms at `NUM_SAMPLES = 5`). The main loop runs continuously — each timer tick triggers one I2C read. Valid reads accumulate; once 5 valid reads are collected, the average is computed and sent to all outputs. This keeps the loop free to service UART at all times.

**Accumulator pattern:** `acc_total` sums valid readings, `acc_count` tracks how many. On reaching `NUM_SAMPLES`, `avg_temp = acc_total / acc_count`, both are reset, and `temp_ready` is set. The flag is cleared immediately after outputs are updated.

**Bad read policy:** A reading is invalid if it is at or below `ADT7420_SENTINEL_THRESHOLD` (-500.0f). Invalid reads are silently skipped — they do not increment `acc_count` or add to `acc_total`. The firmware waits for 5 *valid* readings regardless of how many attempts it takes. Sustained sensor failure means no output is produced (no garbage is sent).

**`TempParts` struct:** Temperature is decomposed into `{sign, whole, frac}` once from the computed average. OLED, Tera Term, and BLE all format from the same two `TempParts` structs (one Celsius, one Fahrenheit). This eliminates any rounding divergence between outputs.

**OLED power toggle:** The OLED starts on at boot. On `%DISCONNECT%`, `BLE_ParseCommand` calls `OLED_Off()` and sets `oled_on = false`. On the next `%STREAM_OPEN%`, if `oled_on` is false, it calls `OLED_RepowerOn()` and sets `oled_on = true`. OLED output in the main loop is gated on `oled_on`.

**Dual terminator buffer:** The UART0 receive buffer watches for two terminators: `\r\n` (normal command/response) and `%...%` (RN4871 module events). Required because RN4871 sends `%STREAM_OPEN%` and `%CONN_PARAM,...%` back-to-back with no `\r\n` between them.

**CMD mode:** When `$$$` is detected on UART1, the firmware enters CMD mode — the sampling timer is suspended, all UART0 output stops, and UART1 input is forwarded to UART0 (with `\n` suppressed). `---` on UART1 exits CMD mode and resets `last_sample` to prevent a stale timer firing immediately.

> **Why blocking averaging was removed:** An earlier version used a `getAverageTemp()` function that called `ADT7420_ReadTemperature()` in a loop with `sleep()` between reads. This blocked UART polling for up to 1 second, making `$$$` CMD mode entry and `START_TEMP` processing unreliable. The non-blocking accumulator pattern in the main loop was the fix. See comment at the bottom of `main.c`.

---

## State Variables

```c
bool streaming  = false;  // app sent START_TEMP; firmware pushes TEMP: packets
bool connected  = false;  // %STREAM_OPEN% received from RN4871
bool cmd_mode   = false;  // $$$ detected on UART1; gates UART0 output and sampling
bool oled_on    = true;   // tracks OLED power state; toggled by %STREAM_OPEN% / %DISCONNECT%

float acc_total  = 0.0f;  // running sum of valid temperature readings
int   acc_count  = 0;     // number of valid readings accumulated so far
bool  temp_ready = false; // set when acc_count reaches NUM_SAMPLES
float avg_temp   = 0.0f;  // computed average, used for all outputs

int dollar_count = 0;  // consecutive $ chars on UART1 (CMD mode entry)
int dash_count   = 0;  // consecutive - chars on UART1 (CMD mode exit)
```

**Constants:**
```c
#define NUM_SAMPLES      5
#define SAMPLE_INTERVAL  (COUNTS_PER_SECOND / NUM_SAMPLES)  // ~200ms per tick
```

---

## Main Loop Structure

```
loop:
  ├── If NOT cmd_mode:
  │     ├── Check timer (fires every SAMPLE_INTERVAL, ~200ms)
  │     │     ├── Read ADT7420 → sample
  │     │     ├── If sample > SENTINEL_THRESHOLD: acc_total += sample; acc_count++
  │     │     └── If acc_count >= NUM_SAMPLES:
  │     │             avg_temp = acc_total / acc_count
  │     │             temp_ready = true; reset acc_total, acc_count
  │     │
  │     └── If temp_ready:
  │             temp_ready = false
  │             If avg_temp is error: xil_printf error; send ERROR:SENSOR_FAIL if streaming
  │             Else:
  │               Decompose avg_temp → temp_c, temp_f (TempParts)
  │               ADT7420_PrintTempParts → Tera Term
  │               If oled_on: OLED_DisplayTempParts → OLED
  │               If streaming: send TEMP:...\r\n over UART0
  │
  ├── Check UART1 (Tera Term) for input:
  │     ├── Echo character to UART1 (\r → \r\n, others echo as-is)
  │     ├── Forward character to UART0 (BLE)
  │     ├── Track $$$ → sets cmd_mode = true
  │     └── If cmd_mode: track --- → sets cmd_mode = false, resets last_sample
  │
  └── Check UART0 (BLE) for input:
        ├── Echo character to UART1 (Tera Term)
        └── If NOT cmd_mode: append to buffer
              ├── On \r\n: pass line to BLE_ParseCommand()
              └── On %...%: pass event to BLE_ParseCommand()
```

---

## `BLE_ParseCommand` Signature

```c
void BLE_ParseCommand(char *buf, bool *connected, bool *streaming,
                      bool *oled_on, OLED_Control_t *my_oled);
```

Dispatches on `buf[0]`:

| First char | Match | Action |
|---|---|---|
| `%` | `%STREAM_OPEN%` | `*connected = true`; if `!*oled_on`: call `OLED_RepowerOn()`, set `*oled_on = true` |
| `%` | `%DISCONNECT%` | `*connected = false`, `*streaming = false`; call `OLED_Off()`, set `*oled_on = false` |
| `S` | `START_TEMP` | If `*connected`: `*streaming = true` |
| `S` | `STOP_TEMP` | `*streaming = false` |
| default | — | Send `ERROR:UNKNOWN_CMD\r\n` over UART0 |

CMD mode detection (`$$$`/`---`) is handled entirely on the UART1 path — `BLE_ParseCommand` never sees those sequences.

---

## OLED Driver — Power Functions

The `oledAddition_v4.0` IP adds a power command register at AXI offset `0x0C` (reg3).

```c
// oled.c

void OLED_RepowerOn(OLED_Control_t *my_oled){
    u32 cmd = 1; 
    
    //Guard: wait for HW to clear bit 1 (is in DONE state) before issuing a new power-on command
    while(Xil_In32(my_oled->base_address +12) & 0x2);
    
    //Set in SW and cleared in Hardware
    Xil_Out32(my_oled->base_address+ 12, 0x2); // Set slv_reg3[1] = 1

    //poll until hardware clears bit 1 - confirms power-on sequence complete
    while(cmd){
        cmd = (Xil_In32(my_oled->base_address + 12) & 0x2);
    }
    
}

void OLED_Off(OLED_Control_t *my_oled){
    u32 cmd = 1; 

    //Guard: wait for HW to clear bit 0 before starting a new power-off command
    while(Xil_In32(my_oled->base_address + 12) & 0x1);

    //Set in SW and clear in HW
    Xil_Out32(my_oled->base_address+ 12, 0x1); // Send reg3[0] = 1

    //Poll until HW clears bit 0 - confirms power-off sequence complete
    while(cmd){
        cmd = (Xil_In32(my_oled->base_address + 12) & 0x1);
    }
```

> **reg3 self-clears in hardware** when the FSM asserts `powerCmdAck` — this happens when the FSM *accepts* the command, not when the physical sequence completes. The 100ms tOFF delay for power-off continues in the PL after the poll returns. This is correct and expected.

Base address macro: `XPAR_OLEDADDITION_0_BASEADDR`

---

## Timer API — Vitis 2025.2

> **Vitis 2025.2 uses `xiltimer.h`, not the legacy `xtime_l.h`.**

```c
#include "xiltimer.h"
#include "xtimer_config.h"

XTime last_sample, now;
XTime_GetTime(&last_sample);

// In loop:
XTime_GetTime(&now);
if ((now - last_sample) >= SAMPLE_INTERVAL) {
    last_sample = now;
    // ... take one sample
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

## Sensor Error Handling

If the ADT7420 read returns a sentinel value, the reading is skipped silently:

```c
sample = ADT7420_ReadTemperature(&i2c);
if (sample > ADT7420_SENTINEL_THRESHOLD) {
    acc_total += sample;
    acc_count++;
}
// No else — bad reads are simply not counted
```

If `temp_ready` is set but `avg_temp` is somehow below the threshold (edge case), the error path fires:

```c
if (avg_temp <= ADT7420_SENTINEL_THRESHOLD) {
    xil_printf("ADT7420: read error ...\r\n");
    if (streaming) XUartPs_Send(&Uart0, (u8*)"ERROR:SENSOR_FAIL\r\n", 19);
} else {
    // normal path: decompose, display, stream
}
```

---

## `temp_display` Integration Layer

`temp_display.c/.h` owns all logic that involves both the ADT7420 driver and the OLED driver together. It is not called in the hot path of `main.c` — `main.c` calls `OLED_DisplayTempParts` directly to avoid recomputing `TempParts`. `temp_display` is provided for convenience when a float is available and decomposition hasn't been done yet.

```c
// Called from main.c (pre-decomposed path — preferred):
OLED_DisplayTempParts(&my_oled, &temp_c, &temp_f);

// Available in temp_display.c (float path — used when TempParts not yet computed):
OLED_DisplayTemp(&my_oled, avg_temp);
```

---

## CMD Mode Detail

**Entry:** Type `$$$` in Tera Term → firmware detects three consecutive `$` on UART1 → sets `cmd_mode = true` → stops sampling timer and all UART0 output → forwards subsequent UART1 input to UART0.

**Exit:** Type `---` in Tera Term → firmware detects three consecutive `-` → sets `cmd_mode = false` → resets `last_sample` via `XTime_GetTime()` to avoid a stale timer firing immediately → resumes normal operation.

**Why `\n` is suppressed in CMD forward path:**  
The RN4871 uses `\r` as its command terminator. Forwarding raw `\r\n` sends a bare `\n` after each command, which the RN4871 interprets as an empty command and responds with `Err`. The firmware sends only `\r`.

---

## Debug Notes

- Bug #1: `xil_printf` routing to BLE → see [bugs_and_fixes.md](bugs_and_fixes.md#bug-1)
- Bug #2: OLED and BLE showing 0.01°F difference → see [bugs_and_fixes.md](bugs_and_fixes.md#bug-2)
- Bug #3: `Err` spam in CMD mode → see [bugs_and_fixes.md](bugs_and_fixes.md#bug-3)
- Bug #4: CMD mode never exiting → see [bugs_and_fixes.md](bugs_and_fixes.md#bug-4)
- Bug #5: Sensor error falling through to `DecomposeTemp` → see [bugs_and_fixes.md](bugs_and_fixes.md#bug-5)

---

## Potential Improvements

- **Interrupt-driven UART:** Current implementation polls UART0 every loop iteration. Interrupt-driven receive with a ring buffer would be cleaner. Deferred — polling is sufficient for this update rate.
- **App state struct:** `BLE_ParseCommand` currently takes four separate pointer parameters. Bundling `connected`, `streaming`, `oled_on`, and `my_oled` into a single app-state struct would clean up the signature and make future additions easier.
- **Dual Pmod BLE support:** Two RN4871 modules and two simultaneous phone connections — noted in `main.c` as a future direction.
- **Forced naming standard:** Header file naming is mostly consistent but not enforced by a convention document. A future pass could standardize prefix usage (`ADT7420_`, `OLED_`, `BLE_`, `UART_`).