# Bugs and Fixes

`Project: BLE-OLED-TMP2 | Ordered chronologically`

---

## Bug #1 — `xil_printf` Output Routed to BLE Instead of Tera Term

**Symptom:** Tera Term shows nothing. nRF Connect receives garbled data (debug strings from `xil_printf`).

**Root cause:** Vitis defaults `standalone_stdin` and `standalone_stdout` to `ps7_uart_0`. UART0 is connected to the RN4871, not the USB-UART. All `xil_printf` output goes to the BLE module.

**Fix:** In Vitis BSP settings, set both `standalone_stdin` and `standalone_stdout` to `ps7_uart_1`. This is a one-time change. Applies to both BSP nodes if there are two.

**Lesson:** Always check BSP stdout assignment when starting a new Vitis project — the default routes output to the wrong UART for this hardware configuration.

---

## Bug #2 — OLED and BLE Temperature Differ by 0.01°F

**Symptom:** OLED displays `74.41F`, BLE sends `74.42F` (or vice versa) for the same sample.

**Root cause:** `celsius_to_fahrenheit()` was called separately in the OLED output path and the BLE output path. Floating-point rounding produced slightly different results from the same raw sensor value depending on the order of operations.

**Fix:** Compute `TempParts` once per cycle from a single ADT7420 read. Pass the same struct to the OLED formatter, `xil_printf`, and the BLE send function. All three outputs now use identical values.

**Lesson:** Any time the same value is displayed in multiple places, compute it once. Calling a conversion function twice on "the same" input does not guarantee identical output with floating-point.

---

## Bug #3 — `Err` Spam from RN4871 During CMD Mode

**Symptom:** Every command typed in Tera Term during CMD mode produces an `Err` response from the RN4871, even valid commands.

**Root cause:** The UART1 forward path was sending raw `\r\n` to UART0. The RN4871 uses `\r` as its command terminator. The bare `\n` that followed each `\r` was interpreted as an empty command, producing `Err`.

**Fix:** In the CMD mode forward path, suppress `\n` — forward only `\r` to UART0.

```c
if (cmd_mode && ch != '\n') {
    XUartPs_SendByte(UART0_BASEADDR, ch);
}
```

**Lesson:** When bridging between two UART channels with different line ending conventions, strip the unwanted terminator at the forward point.

---

## Bug #4 — CMD Mode Never Exits

**Symptom:** Typing `---` in Tera Term has no effect. CMD mode stays active permanently.

**Root cause:** `---` detection was placed inside `parse_command()`, which is gated behind `!cmd_mode`. While in CMD mode, UART0 input never reaches `parse_command`, so `---` was never seen.

**Fix:** Move `---` detection to the UART1 receive path using `dash_count`, the same way `$$$` detection uses `dollar_count`. Exit detection runs regardless of `cmd_mode` state.

**Lesson:** Entry and exit detection for a mode must live on the same code path. Gating the exit check behind the mode it's trying to exit is a logical deadlock.

---

## Bug #5 — Sensor Error Falls Through to `DecomposeTemp`

**Symptom:** On ADT7420 read failure, garbage values appear on the OLED and are sent over BLE instead of an error message.

**Root cause:** The sensor read error check used an `if` without an `else`. After sending `ERROR:SENSOR_FAIL\r\n`, execution continued to `DecomposeTemp` with an invalid value.

**Fix:** Use `if/else` — error path sends the error message and returns; normal path decomposes and displays.

```c
if (temp_raw == ERROR_SENTINEL) {
    if (streaming) send_uart0("ERROR:SENSOR_FAIL\r\n");
    // skip everything else
} else {
    DecomposeTemp(temp_raw, &parts);
    // OLED, xil_printf, BLE send
}
```

**Lesson:** When a fallthrough would corrupt downstream state, use `if/else` — not `if` + implicit continuation.

---

## Bug #6 — Android App Never Receives BLE Scan Results

**Symptom:** App scans but the device list is always empty. BLE device visible in nRF Connect on the same phone.

**Root cause:** On Android 12+, `BLUETOOTH_SCAN` and `BLUETOOTH_CONNECT` must be requested at runtime in addition to being declared in `AndroidManifest.xml`. The manifest entries alone are not sufficient. The only permission showing in the app's system settings was Location — Bluetooth permissions had never been requested.

**Fix:** Add `permission_handler: ^11.3.1` to `pubspec.yaml`. Request `Permission.bluetoothScan` and `Permission.bluetoothConnect` at runtime before scanning, wrapped in `Platform.isAndroid`.

**Lesson:** Android 12+ has a split between "declared" and "granted" permissions for Bluetooth. Declaring in the manifest is not the same as requesting at runtime. Check the app's permission list in system settings if scan results are empty.

---

## Bug #7 — iOS Redirects to Settings on Every Connect Tap

**Symptom:** On iPhone, tapping Connect immediately opens iOS Settings instead of scanning.

**Root cause:** `permission_handler` returns `PermissionStatus.permanentlyDenied` for Android-specific Bluetooth permission keys (`Permission.bluetoothScan`, `Permission.bluetoothConnect`) on iOS. The app logic interpreted this as a denied permission and redirected to Settings.

**Fix:** Wrap the entire Android runtime permission block in `Platform.isAndroid` (from `dart:io`). iOS never executes this code path. iOS Bluetooth permission is handled exclusively by the `NSBluetoothAlwaysUsageDescription` system prompt at first launch.

```dart
if (Platform.isAndroid) {
  // permission_handler logic here
}
```

**Lesson:** `permission_handler` is not cross-platform for BLE keys. Android-specific permissions return incorrect statuses on iOS. Always guard Android permission code with `Platform.isAndroid`.

---

## Bug #8 — NDK Version Mismatch After Adding flutter_blue_plus

**Symptom:**
```
Your project is configured with Android NDK 26.3.11579264, but the following
plugin(s) depend on a different Android NDK version:
- flutter_blue_plus_android requires Android NDK 27.0.12077973
```

**Root cause:** The default Flutter project is configured with NDK 26. `flutter_blue_plus_android` requires NDK 27.

**Fix:** In `android/app/build.gradle.kts`, add inside the `android {}` block:
```kotlin
ndkVersion = "27.0.12077973"
```
Install NDK 27 via Android Studio SDK Manager → SDK Tools → NDK if not already present.

**Lesson:** When adding BLE plugins, check NDK requirements immediately. The error message includes the exact version to set.

---

## Bug #9 — iOS BLE Stack Completely Non-Functional

**Symptom:** App builds and runs on iPhone. Scan finds nothing. Connection attempts fail silently. Same code works on Android.

**Root cause:** `pod install` had never been run on the Mac. CocoaPods native BLE libraries were not linked into the iOS build. The Flutter build succeeded (it doesn't require pods to compile) but the BLE native layer was absent at runtime.

**Fix:**
```bash
cd flutter/ble_temp_app/ios
pod install
cd ..
flutter run
```

Also: uncomment `platform :ios, '13.0'` in the Podfile before running `pod install`.

After installing pods, delete the app from the iPhone and reinstall — the old build doesn't have BLE linked.

**Lesson:** Flutter BLE plugins require native CocoaPods integration on iOS. `flutter run` does not run `pod install` automatically. If iOS BLE is completely non-functional and Android works fine, `pod install` is the first thing to check.

---

## Bug #10 — Scan Loop Race Condition on Retry

**Symptom:** After tapping Retry, the scan appears to start but immediately stops. Second tap works correctly.

**Root cause:** The scan restart logic listened to the `isScanning` stream and called `startScan()` when it emitted `false`. On retry, `isScanning` emitted `false` before the new scan had registered as started, causing the stop listener to fire and cancel the new scan.

**Fix:** Use a local `scanHasStarted` boolean flag. Set it to `true` immediately when `startScan()` is called. Only call `stopScan()` if `scanHasStarted` is true and the current result isn't the target device. Also call `stopScan()` before `startScan()` on every retry to ensure a clean slate.

**Lesson:** BLE scanning state streams are asynchronous and don't reflect the scan state instantaneously. Don't rely on stream emissions alone to gate scan start/stop logic — use a local flag as a source of truth.

---

## Bug #11 — Stale Error Message on Reconnect

**Symptom:** After an unexpected disconnect and successful auto-reconnect, a previous error message from the last session is still visible on the streaming screen.

**Root cause:** `_errorMsg` was not cleared when re-entering the streaming state via reconnect. It was only cleared on a fresh Connect from `disconnected`.

**Fix:** Clear `_errorMsg` at the top of `_discoverAndSubscribe()` so every reconnect starts with a clean error state.

**Lesson:** Any state that should reset on reconnect must be explicitly cleared on the reconnect path. Don't assume that paths taken only on fresh connections cover all re-entry cases.

---

## Bug #12 — Corrupted Vitis Workspace

**Symptom:**
```
CMake Error at UserConfig.cmake:202 (ndif): Unknown CMake command "ndif".
```
Clean fails. Rebuild fails. Workspace appears broken regardless of what is changed.

**Root cause:** The initial workspace (`ws/`) entered a corrupted CMake state. The `ndif` error indicates a malformed generated CMake file — not a source code issue.

**Fix:** Create a new workspace (`ws_1/`). Do not attempt to repair a corrupted Vitis workspace — the nuclear option is faster.

**Lesson:** If a Vitis workspace produces CMake errors that don't match any source file issue, discard the workspace and start a new one. Workspaces are cheap to recreate; debugging CMake internals is not.

---

## Bug #13 — `lscript_a9.ld.in Does Not Exist`

**Symptom:**
```
CMake Error: File .../src/linker_files/lscript_a9.ld.in does not exist.
CMake Error at Findcommon.cmake:197 (configure_file): configure_file Problem configuring file
```

**Root cause:** The application was built before the linker script was generated. Vitis requires an explicit **Reset Linker Script** step to generate `lscript_a9.ld.in` before the first build. This is not done automatically when source files are added.

**Fix:** Right-click the application in the Vitis Explorer → **Reset Linker Script** → then build. Must be done before the first build on any new application.

Reference: [AMD Vitis Linker Script Documentation](https://docs.amd.com/r/en-US/ug1400-vitis-embedded/Linker-Scripts?tocId=nelEb6zIWCPFFjsSi9EysA)

**Lesson:** Always reset the linker script before the first build of a new Vitis application. The error message is about a missing generated file, not a missing source file — the distinction matters for diagnosis.

---

## Bug #14 — Multiple Definition Linker Errors

**Symptom:**
```
multiple definition of `Uart0'
multiple definition of `Uart1'
multiple definition of `main'
multiple definition of `init_uart'
```

**Root cause:** A stale `.c` file was still present in `src/` alongside the current source files. Vitis compiles every `.c` file in `src/` with no concept of inactive files. Two files defining the same symbols causes linker collisions.

**Fix:** Remove any stale `.c` files from `src/`. Rebuild.

**Lesson:** Vitis compiles all `.c` files in `src/` unconditionally. There is no way to exclude a file without removing it. Keep only the files that belong to the current build in `src/`.
