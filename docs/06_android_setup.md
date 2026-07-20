# 05 — Android Setup

`Project: BLE-OLED-TMP2 | Dev machine: Windows | Test device: Moto G8 Power`

---

## Overview

Android development for this project runs on Windows using VS Code and the Android SDK. The steps below cover environment setup, Flutter project creation, NDK fix, BLE permission configuration, and running on a physical device.

---

## Prerequisites

- Flutter SDK installed and on PATH (`flutter doctor` passes)
- Android Studio installed (used for SDK management, not necessarily as the IDE)
- Android SDK 36.0.0
- Android Studio 2025.1.1
- Physical Android device (Moto G8 Power used for testing)

> **To test BLE connectivity, the ZedBoard must be powered on with the Vitis application running and the Pmod BLE set up and advertising (power LED blinking).** See [ZedBoard-BLE](https://github.com/Ava-Kirkland/ZedBoard-BLE) for Pmod BLE setup and [02_vitis_setup.md](02_vitis_setup.md) for ZedBoard hardware setup.

---

## Developer Mode on Windows

Flutter requires symlink support. If you see:

```
Building with plugins requires symlink support.
Please enable Developer Mode in your system settings.
```

Run `start ms-settings:developers` in a terminal and enable Developer Mode.

---

## Create the Flutter Project

```bash
flutter create ble_temp_app
cd ble_temp_app
flutter pub add flutter_blue_plus
flutter pub add permission_handler
flutter pub get
```

---

## NDK Version Fix

After adding `flutter_blue_plus`, the build will fail with:

```
Your project is configured with Android NDK 26.3.11579264, but the following
plugin(s) depend on a different Android NDK version:
- flutter_blue_plus_android requires Android NDK 27.0.12077973
```

**Fix:** In `android/app/build.gradle.kts`, add `ndkVersion` inside the `android {}` block:

```kotlin
android {
    namespace = "com.example.ble_temp_app"
    compileSdk = flutter.compileSdkVersion
    ndkVersion = "27.0.12077973"   // ← add this

    defaultConfig {
        applicationId = "com.example.ble_temp_app"
        minSdk = 21                // ← change from flutter.minSdkVersion
        targetSdk = flutter.targetSdkVersion
        versionCode = flutter.versionCode
        versionName = flutter.versionName
    }
}
```

> `minSdk = 21` is required by `flutter_blue_plus`. The default Flutter value is lower than this.

To install NDK 27 if not present: Android Studio → SDK Manager → SDK Tools → NDK → install version 27.0.12077973.

---

## AndroidManifest.xml — BLE Permissions

Add the following inside `<manifest>`, after the opening `<manifest>` tag and before `<application>`:

```xml
<!-- Required for BLE scanning and connecting -->
<uses-feature android:name="android.hardware.bluetooth_le" android:required="false" />

<!-- Android 12+ runtime Bluetooth permissions -->
<uses-permission android:name="android.permission.BLUETOOTH_SCAN"
    android:usesPermissionFlags="neverForLocation" />
<uses-permission android:name="android.permission.BLUETOOTH_CONNECT" />

<!-- Legacy: Android 11 and below -->
<uses-permission android:name="android.permission.BLUETOOTH"
    android:maxSdkVersion="30" />
<uses-permission android:name="android.permission.BLUETOOTH_ADMIN"
    android:maxSdkVersion="30" />
<uses-permission android:name="android.permission.ACCESS_FINE_LOCATION"
    android:maxSdkVersion="30" />

<!-- Legacy: Android 9 and below -->
<uses-permission android:name="android.permission.ACCESS_COARSE_LOCATION"
    android:maxSdkVersion="28" />
```

> **Manifest entries alone are not sufficient on Android 12+.** Runtime permission requests are also required in code. This is handled in `main.dart` via `permission_handler` — see [04_flutter_app.md](04_flutter_app.md).

---

## Running on a Physical Device

1. Enable USB Debugging on the Android device: Settings → Developer Options → USB Debugging
2. Connect via USB and accept the connection prompt on the device
3. In VS Code, open `lib/main.dart`
4. Start a debug session and select the Android device from the device picker

```bash
flutter run
```

---

## Debugging Notes

- Bug #6: Android runtime permissions missing → app never received BLE device list → see [bugs_and_fixes.md](bugs_and_fixes.md#bug-6--android-app-never-receives-ble-scan-results)
- Bug #8: NDK mismatch after adding flutter_blue_plus → see [bugs_and_fixes.md](bugs_and_fixes.md#bug-8--ndk-version-mismatch)
