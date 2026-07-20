# 06 — iOS Setup (macOS)

`Project: BLE-OLED-TMP2 | Dev machine: macOS (Apple Silicon) | Flutter: 3.41.9 | Xcode: 16.4`

---

## Overview

iOS builds require a Mac. The Mac clone of the repo exists solely for iOS development — the Vivado/Vitis projects and Android development both stay on Windows. This doc covers the problems encountered getting from a fresh Mac to a working Flutter app on a physical iPhone.

---

## Prerequisites

- macOS with Xcode 16.4 installed (from the App Store)
- Flutter installed (see Step 1 below if not set up)
- Physical iPhone for testing
- Repo cloned on the Mac

> **To test BLE connectivity, the ZedBoard must be powered on with the Vitis application running and the Pmod BLE set up and advertising (power LED blinking).** See [ZedBoard-BLE](https://github.com/Ava-Kirkland/ZedBoard-BLE) for Pmod BLE setup and [02_vitis_setup.md](02_vitis_setup.md) for ZedBoard hardware setup.

---

## Step 1 — Flutter PATH Setup

If `flutter` is not recognized after installation:

**Diagnose:**
```bash
find ~ -name "flutter" -type f 2>/dev/null
```

**Fix — add Flutter to PATH:**
```bash
echo 'export PATH="/path/to/flutter/bin:$PATH"' >> ~/.zshrc
source ~/.zshrc
```

> **Avoid spaces in directory names for dev tools.** A path like `/Users/ava/flutter for VS code/` breaks PATH resolution. Rename the directory before setting PATH:
> ```bash
> mv "/Users/ava/flutter for VS code" "/Users/ava/flutter"
> ```

> **Check the nesting level.** The actual Flutter binary may be nested deeper than expected. If PATH is set correctly but `flutter` still isn't found, verify:
> ```bash
> # Wrong assumption:
> /Users/ava/flutter/bin/flutter
> # Actual location (nested):
> /Users/ava/flutter/flutter/bin/flutter
> ```
> Use `find` output to confirm the exact path.

**Verify:**
```bash
flutter doctor
```

Expected (iOS-only dev machine — Android warnings are ignorable):
```
[✓] Flutter (Channel stable, 3.41.9, ...)
[!] Android toolchain    ← ignore
[✓] Xcode - develop for iOS and macOS (Xcode 16.4)
[✓] Connected device
[✓] Network resources
```

---

## Step 3 — Info.plist — BLE Permission

Add the Bluetooth usage description to `ios/Runner/Info.plist`:

```xml
<key>NSBluetoothAlwaysUsageDescription</key>
<string>This app uses Bluetooth to connect to the ZedBoard temperature sensor.</string>
```

iOS shows this string in the system Bluetooth permission prompt on first launch. Without it, the app crashes when it tries to use BLE.

---

## Step 4 — Podfile — iOS Minimum Version

In `ios/Podfile`, uncomment the platform line and set it to 13.0:

```ruby
platform :ios, '13.0'
```

`flutter_blue_plus` requires iOS 13.0 minimum. The default Podfile leaves this commented out, which causes build failures.

---

## Step 5 — `pod install` (Required Before First Build)

```bash
cd flutter/ble_temp_app/ios
pod install
cd ..
```

> **This step is not optional and is not done automatically by Flutter.** Skipping it means the BLE native libraries are not linked. Symptoms look like BLE stack issues (scan never finds devices, connection fails silently) rather than a missing dependency. Run `pod install` once on any new Mac before the first `flutter run`.

After `pod install`, if you had previously built and installed the app on the iPhone, delete it from the device and reinstall. The old build didn't have BLE linked.

---

## Step 6 — Xcode Code Signing

Deploying to a physical iPhone requires a signing certificate.

**Open the iOS project:**
```bash
open ios/Runner.xcworkspace
```

> Always open `.xcworkspace`, never `.xcodeproj`. CocoaPods requires the workspace file.

**Configure signing in Xcode:**
1. Select **Runner** in the left navigator
2. Select the **Runner** target
3. Go to **Signing & Capabilities** tab
4. Sign in with your Apple ID under Team
5. Enable **Automatically manage signing**
6. Set a unique **Bundle Identifier**

**Register the device:**
- Plug iPhone in via USB
- Xcode will prompt to register — accept
- On the iPhone: Settings → Privacy & Security → Developer Mode → enable → restart the phone

**Trust the certificate on the iPhone:**
- Settings → General → VPN & Device Management → your developer certificate → Trust
- If this prompt appears after `flutter run` has already started, cancel the run, complete the trust step, then run again

**Run:**
```bash
flutter run
```

### Apple Developer Account Options

| Account Type | Cost | Certificate Expiry | Notes |
|---|---|---|---|
| Free Apple ID | $0 | 7 days | Re-sign required weekly; 3 app limit |
| Apple Developer Program | $99/year | No expiry | Unlimited apps, TestFlight, App Store |

A free Apple ID is sufficient for development testing.

---

## Quick Reference — iOS Build Checklist

Before every iOS build session on a new Mac or after a long break:

- [ ] `flutter doctor` shows ✅ Flutter, ✅ Xcode, ✅ Connected device
- [ ] `pod install` has been run in `ios/` at least once
- [ ] Xcode signing team is configured
- [ ] iPhone is trusted on the device
- [ ] Open `Runner.xcworkspace` (not `.xcodeproj`) if launching Xcode manually

---

## Debug Notes

- Step 1/2: Flutter not found / wrong PATH nesting → see above
- Step 3: No code signing certificate → Xcode signing setup → see above
- Bug #7: iOS always redirecting to Settings → `Platform.isAndroid` guard → see [bugs_and_fixes.md](bugs_and_fixes.md#bug-7--ios-redirects-to-settings-on-every-connect-tap)
- Bug #9: iOS BLE completely non-functional → `pod install` never ran → see [bugs_and_fixes.md](bugs_and_fixes.md#bug-9--ios-ble-stack-completely-non-functional)
