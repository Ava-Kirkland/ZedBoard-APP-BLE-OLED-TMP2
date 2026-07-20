# Troubleshooting — Quick Reference

`Project: BLE-OLED-TMP2 | Symptom → Cause → Fix`

---

## Firmware / Vitis

### Tera Term shows nothing; nRF Connect receives garbled text

**Cause:** `standalone_stdout` set to `ps7_uart_0` (BLE UART) instead of `ps7_uart_1`.  
**Fix:** Vitis BSP settings → set `standalone_stdin` and `standalone_stdout` to `ps7_uart_1`. Rebuild. This resets after every platform rebuild — check it first.

---

### OLED and BLE show different temperature values (0.01° apart)

**Cause:** `celsius_to_fahrenheit()` called separately in OLED path and BLE path — floating-point rounding diverges.  
**Fix:** Compute `TempParts` once per cycle from a single sensor read. Pass the struct to all three output paths (OLED, Tera Term, BLE).

---

### RN4871 responds with `Err` to every command in CMD mode

**Cause:** UART1 forward path sending `\r\n` — bare `\n` after `\r` is treated as an empty command.  
**Fix:** Suppress `\n` in the CMD mode forward path. Forward `\r` only.

---

### CMD mode doesn't exit when `---` is typed

**Cause:** `---` detection is inside `parse_command()`, which is gated behind `!cmd_mode`.  
**Fix:** Move `---` detection to the UART1 receive path using a `dash_count` counter, same pattern as `dollar_count` for entry.

---

### Sensor failure produces garbage OLED/BLE output instead of error message

**Cause:** Error check uses `if` without `else` — execution falls through to `DecomposeTemp` with invalid data.  
**Fix:** Use `if/else` — error path returns early; normal path continues.

---

### BLE not streaming after reconnect, or stale error message visible after reconnect

**Cause:** BLE subscriptions and streaming state do not persist across a disconnect. Reconnect must re-run service discovery and re-send `START_TEMP`.  
**Fix:** Call `_discoverAndSubscribe()` on reconnect. Clear `_errorMsg` at the top of that function.

---

## Android

### Build fails: NDK version mismatch after adding flutter_blue_plus

**Error:**
```
flutter_blue_plus_android requires Android NDK 27.0.12077973
```
**Fix:** Add `ndkVersion = "27.0.12077973"` to `android/app/build.gradle.kts` inside `android {}`. Install NDK 27 via Android Studio SDK Manager.

---

### Compile error on `device.connect()`

**Error:** Missing required parameter or incorrect signature.  
**Fix:** `await device.connect(license: License.nonprofit);` — `license` parameter is required in flutter_blue_plus 2.3.10.

---

### App scans but BLE device list is always empty (Android)

**Cause:** Android 12+ requires runtime permission requests for `BLUETOOTH_SCAN` and `BLUETOOTH_CONNECT`. Manifest declarations alone are not enough.  
**Fix:** Use `permission_handler` to request both at runtime, wrapped in `Platform.isAndroid`.  
**Verify:** Check the app's permission list in Android system settings — Bluetooth should appear there, not just Location.

---

### Flutter project won't build: symlink error

**Error:**
```
Building with plugins requires symlink support. Please enable Developer Mode.
```
**Fix:** Run `start ms-settings:developers` → enable Developer Mode on Windows.

---

### `minSdk` build error after adding flutter_blue_plus

**Cause:** Default Flutter `minSdk` is below 21; `flutter_blue_plus` requires API 21+.  
**Fix:** In `android/app/build.gradle.kts`, set `minSdk = 21` explicitly inside `defaultConfig {}`.

---

## iOS

### iOS BLE is completely non-functional — scan finds nothing, connection fails silently

**Cause:** `pod install` was never run on this Mac. BLE native libraries not linked.  
**Fix:**
```bash
cd flutter/ble_temp_app/ios
pod install
cd ..
```
Delete app from iPhone, reinstall. This is a one-time setup per machine.

---

### iOS redirects to Settings immediately on every Connect tap

**Cause:** `permission_handler` returns `permanentlyDenied` for Android BLE keys on iOS.  
**Fix:** Wrap the entire Android runtime permission block in `Platform.isAndroid`. iOS never executes that code path.

---

### `flutter` command not found on Mac

**Cause:** Flutter `bin` directory not on PATH, or path contains a space, or nested one level deeper than expected.  
**Fix:** Run `find ~ -name "flutter" -type f` to locate the binary. Add the correct `bin` directory to `~/.zshrc`. See [06_ios_setup.md](06_ios_setup.md).

---

### No valid code signing certificate for physical iPhone

**Cause:** Xcode not configured with a signing team.  
**Fix:** Open `Runner.xcworkspace` in Xcode → Runner target → Signing & Capabilities → add Apple ID team → enable automatic signing → set unique bundle ID → register device.

---

## Nuclear Options

| Problem | Nuclear option |
|---|---|
| Vitis build in a broken state that targeted fixes don't resolve | Create a new Vitis workspace, re-import the platform and application |
| iOS CocoaPods in an inconsistent state | Delete `ios/Pods/`, `ios/Podfile.lock`, and the app from the iPhone. Re-run `pod install`. |
| Flutter pub cache corrupted | `flutter clean` then `flutter pub get` |
