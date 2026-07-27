# Troubleshooting

`Project: APP-BLE-OLED-TMP2`

Quick-reference lookup by symptom. Each entry gives the most likely cause first. For full root cause and fix details, see [bugs_and_fixes.md](bugs_and_fixes.md).

---

## OLED Issues

### OLED shows nothing on startup

1. **`standalone_stdout` misconfigured** — BSP stdout is set to `ps7_uart_0` instead of `ps7_uart_1`. The OLED IP never gets initialized because the firmware is routing debug output to the BLE module. Fix: set both BSP nodes to `ps7_uart_1`. See [Bug #1](bugs_and_fixes.md#bug-1).
2. **Wrong base address macro** — software is using the original tutorial OLED IP macro instead of `XPAR_OLEDADDITION_0_BASEADDR`. Fix: verify the macro in `main.c` matches the IP name in Vivado.
3. **Board Initialization not set to FSBL** — AXI IP is unresponsive. Fix: set Board Initialization to FSBL in Vitis application settings.
4. **Port name mismatch in constraints** — Vivado added `_0` suffix to external ports; XDC file uses original names. Fix: rename ports manually in block design to remove `_0`.

### OLED stays off after phone connects

**Cause:** `oled_on` flag was `false` when `%STREAM_OPEN%` was received but the `OLED_RepowerOn()` call in `BLE_ParseCommand` was not reached. Check the `oled_on != NULL && my_oled != NULL && (!(*oled_on))` guard in the `%STREAM_OPEN%` handler. Both pointers must be non-NULL and `oled_on` must be `false` to trigger repower.

### OLED turns off unexpectedly

**Cause:** `%DISCONNECT%` was received from the RN4871 — this is expected behavior. The OLED powers off on BLE disconnect and back on when the phone reconnects.

### OLED and BLE temperature values differ by 0.01°F

**Cause:** `celsius_to_fahrenheit()` is being called separately in multiple output paths. Fix: compute `TempParts` once from the average, share the struct. See [Bug #2](bugs_and_fixes.md#bug-2).

---

## Temperature / Sampling Issues

### No temperature output — OLED blank and BLE sends nothing

1. **Sensor fail with all bad reads** — all 5 sample slots are being skipped (all reads return sentinel values). Check I2C wiring, pull-up resistors, and Pmod TMP2 address jumpers (JP1=JP2 open for address `0x4B`).
2. **`acc_count` never reaches `NUM_SAMPLES`** — timer is not firing. Check `SAMPLE_INTERVAL` computation and that `!cmd_mode` is not permanently true.
3. **OLED off** — the OLED is powered down (phone disconnected). Temperature is still being computed and printed to Tera Term even when the OLED is off — verify Tera Term output first.

### Temperature output is frozen / stuck on one value

**Cause:** `temp_ready` flag is not being cleared. Verify `temp_ready = false` is the first line inside the `if (temp_ready)` block in `main.c`.

### Temperature updates are slower than ~1 second

**Cause:** Bad reads are delaying accumulation. If the sensor intermittently returns sentinels, `acc_count` takes more than 5 timer ticks to reach `NUM_SAMPLES`. Check sensor connection.

### Temperature output resumes unexpectedly after a stale delay when exiting CMD mode

**Cause:** `last_sample` was not reset on CMD mode exit, so the timer fires immediately on the first loop iteration. Fix: call `XTime_GetTime(&last_sample)` when `cmd_mode` is set to `false`. See [Bug #4](bugs_and_fixes.md#bug-4).

---

## BLE / UART Issues

### RN4871 responds with `Err` spam on startup

**Cause:** `standalone_stdout` is set to `ps7_uart_0` — `xil_printf` output is going to the RN4871. Fix: set BSP stdout to `ps7_uart_1`. See [Bug #1](bugs_and_fixes.md#bug-1).

### RN4871 responds with `Err` spam during normal operation

**Cause:** Firmware is sending temperature data over UART0 while in CMD mode. Fix: gate all UART0 output on `cmd_mode == false`. See [Bug #3](bugs_and_fixes.md#bug-3).

### CMD mode (`$$$`) entry is unreliable — firmware misses the sequence

**Cause:** A blocking call (e.g., `sleep()`) inside the main loop is dropping UART1 characters while the CPU is blocked. Fix: remove all blocking calls from the main loop. Temperature accumulation must be non-blocking. See [Bug #6](bugs_and_fixes.md#bug-6).

### CMD mode never exits after `---`

**Cause:** `dash_count` is being reset by the UART0 forward logic before it reaches 3. Fix: check and update `dash_count` / `cmd_mode` before forwarding. See [Bug #4](bugs_and_fixes.md#bug-4).

### `START_TEMP` from the phone is dropped intermittently

**Cause:** Blocking calls in the main loop (e.g., `sleep()` in an averaging function) are preventing UART0 polling. By the time the loop checks UART0, the character has been lost. Fix: non-blocking accumulation in the main loop. See [Bug #6](bugs_and_fixes.md#bug-6).

### UART init fails — no output at all

**Cause:** `XUartPs_LookupConfig` is being called with `DEVICE_ID` instead of `BASEADDR`. In Vitis 2025.2, pass `XPAR_XUARTPS_0_BASEADDR` / `XPAR_XUARTPS_1_BASEADDR`. See [tool_version_differences.md](tool_version_differences.md#2-xuartpslookupconfig-takes-a-base-address-not-a-device-id).

---

## Flutter / iOS / Android Issues

### iOS BLE redirects to Settings on every Connect tap

**Cause:** `pod install` was never run on the Mac. BLE native libraries are not linked. Fix: run `pod install` in `flutter/ble_temp_app/ios/` once before the first iOS build.

### iOS BLE does not work at all (no scan, no connect)

**Cause:** Same as above — `pod install` not run. Also verify `Runner.xcworkspace` (not `Runner.xcodeproj`) was opened in Xcode.

### Android scan finds no devices

**Cause:** Runtime Bluetooth permissions not requested. Verify `permission_handler` is initializing permissions on Android before scanning begins.

### `device.connect()` compile error

**Cause:** Missing `license: License.nonprofit` parameter. Add it: `await device.connect(license: License.nonprofit)`. See [tool_version_differences.md](tool_version_differences.md#8-deviceconnect-requires-license-licensenonprofit).

---

## Build Issues

### Missing symbols for OLED / ADT7420 / UART functions

**Cause:** Driver `.c` files not copied into `vitis/src/`. Copy all `.c` and `.h` files manually — Vitis 2025.x TCL auto-loading is broken for custom IP. See [tool_version_differences.md](tool_version_differences.md#4-custom-ip-driver-files-must-be-manually-copied-to-src).

---

## Nuclear Options

| Problem | Nuclear option |
|---------|---------------|
| Vitis build errors that don't respond to clean/rebuild | Delete workspace, re-create from `.xsa`, re-copy driver files |
| iOS BLE completely non-functional | Delete `ios/Pods/`, re-run `pod install`, rebuild |
| RN4871 in unknown state | Power-cycle ZedBoard; send `$$$` in Tera Term to enter CMD mode, verify with `D` command |
| Vivado synthesis won't launch via GUI | Use Tcl console: `reset_run synth_1; launch_runs synth_1 -jobs 4` |