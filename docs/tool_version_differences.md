# Tool Version Differences

`Project: BLE-OLED-TMP2`  
`Tools: Vitis 2025.2 | flutter_blue_plus 2.3.10 | Android NDK 27`

Differences from documentation or older tutorials that were encountered during this project.

---

## 1. Vitis 2025.2 — Timer API: `xiltimer.h` Replaces `xtime_l.h`

**Older docs/tutorials show:** `#include "xtime_l.h"` with `XTime_GetTime()` and `XTIME_CLK_TICKS_PER_SEC`

**Vitis 2025.2 behavior:** Use `#include "xiltimer.h"`. `COUNTS_PER_SECOND` is defined in `xtimer_config.h` as `XPAR_CPU_CORE_CLOCK_FREQ_HZ / 2`.

**Impact:** Code using `xtime_l.h` may compile but the constant names differ.

**Fix — confirmed working:**
```c
#include "xiltimer.h"

XTime now;
XTime_GetTime(&now);
if ((now - last_sample) >= COUNTS_PER_SECOND) { ... }
```

---

## 2. Vitis 2025.2 — `standalone_stdout` Resets After Platform Rebuild

**Expected behavior:** BSP defaults route output to a sensible UART.

**Vitis 2025.2 behavior:** `standalone_stdin` and `standalone_stdout` default to `ps7_uart_0`. For this hardware, UART0 is the BLE module — not the USB-UART used for Tera Term. **Confirmed behavior in this project.**

**Impact:** Tera Term shows nothing; `xil_printf` output goes to the BLE module instead.

**Fix — confirmed working:** Set both to `ps7_uart_1` in BSP settings. This is a one-time change per project. Check it first if Tera Term goes silent after a fresh project setup.

---

## 3. flutter_blue_plus 2.3.10 — `device.connect()` Requires `license` Parameter

**Older docs/tutorials show:** `await device.connect();`

**flutter_blue_plus 2.3.10 behavior:** `connect()` requires a `license` parameter. Omitting it is a compile error.

**Impact:** Breaking change from earlier API versions.

**Fix — confirmed working:**
```dart
await device.connect(license: License.nonprofit);
```

`License.nonprofit` is the correct value for non-commercial use.

---

## 4. flutter_blue_plus 2.3.10 — NDK 27 Required

**Default Flutter project:** Configured with NDK 26.

**flutter_blue_plus_android 2.3.10 behavior:** Requires NDK 27.0.12077973.

**Impact:** Build fails with NDK version mismatch error after adding the plugin.

**Fix — confirmed working:** Set `ndkVersion = "27.0.12077973"` in `android/app/build.gradle.kts`. Install NDK 27 via Android Studio SDK Manager.

---

## 5. flutter_blue_plus — `permission_handler` Returns Wrong Values for BLE Keys on iOS

**Expected behavior:** `permission_handler` returns accurate permission status cross-platform.

**Actual behavior:** Returns `PermissionStatus.permanentlyDenied` for `Permission.bluetoothScan` and `Permission.bluetoothConnect` on iOS, regardless of actual permission state.

**Impact:** iOS app immediately redirects to Settings on Connect tap.

**Fix — confirmed working:** Wrap the entire Android runtime permission block in `Platform.isAndroid`. iOS permission is handled by the system prompt via `Info.plist` — the app should never call `permission_handler` for BLE keys on iOS.

---

## 6. Android 12+ — Runtime BLE Permissions Required in Addition to Manifest

**Pre-Android 12 behavior:** Declaring permissions in `AndroidManifest.xml` was sufficient.

**Android 12+ behavior:** `BLUETOOTH_SCAN` and `BLUETOOTH_CONNECT` must also be requested at runtime via code. Manifest-only is insufficient — the permissions will not be granted.

**Impact:** App compiles and runs, but BLE scan returns no results. Permission list in system settings shows only Location, not Bluetooth.

**Fix — confirmed working:** Use `permission_handler` to request both permissions at runtime before scanning, gated with `Platform.isAndroid`.

---

## 7. iOS — `pod install` Not Run Automatically by Flutter

**Expected behavior (reasonable assumption):** `flutter run` handles all native dependency setup.

**Actual behavior:** CocoaPods dependencies must be installed manually via `pod install` in the `ios/` directory before the first build on a new machine. `flutter run` does not do this.

**Impact:** BLE is completely non-functional on iOS. Symptoms look like BLE stack issues, not a missing dependency.

**Fix — confirmed working:**
```bash
cd ios/
pod install
cd ..
flutter run
```

Run once per machine. Re-run if pods are updated.

---

## 8. `oledAddition_v3.0` base address macro differs from original OLED IP

**Tutorial/docs behavior:** Original OLED IP tutorial uses the macro generated for the original IP name.

**2025.2 behavior:** When `oledAddition_v3.0` is added to the block design, Vivado generates `XPAR_OLEDADDITION_0_BASEADDR` as the base address macro (not the original tutorial IP's macro name).

**Impact:** Using the wrong macro compiles silently but the driver writes to the wrong AXI address, producing no OLED output.

**Fix:** Use `XPAR_OLEDADDITION_0_BASEADDR` in all software that references the OLED base address. **Confirmed working.**

---

## 9. Board Initialization must be set to FSBL in Vitis for custom IP

**Tutorial/docs behavior:** Not always mentioned in tutorials for simple projects.

**2025.2 behavior:** Custom IP applications require Board Initialization set to FSBL. Without it, the PS-PL interface may not be correctly initialized and the AXI IP will be unresponsive.

**Impact:** OLED produces no output despite correct software and bitstream.

**Fix:** In Vitis application settings, set Board Initialization to FSBL. **Confirmed working.**

---
