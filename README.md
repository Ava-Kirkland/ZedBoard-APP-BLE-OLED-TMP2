# BLE-OLED-TMP2

**Engineer:** Ava Kirkland — Nspired Engineering  
**Status:** ✅ V1 Complete — validated on Android and iOS

A ZedBoard (Zynq-7000) reads temperature from a Pmod TMP2 (ADT7420) over I2C, displays it on the onboard OLED via a custom AXI-Lite IP, and streams live readings to a Flutter mobile app over Bluetooth Low Energy via a Pmod BLE (RN4871). The phone app displays °C and °F simultaneously, updating at 1Hz.

---

## Results

<table>
<tr>
<td><img src="images/Sensors-only.jpeg" alt="Zoomed in view of Sensors and Peripherials" width="600"> </td>
</tr>
<tr><td><img src="images/Android-RoomTemp.jpeg" alt ="System when sensing room temperature" width="600"> <br> The System at the Temperature of the Room</td></tr>
<tr><td><img src="images/Android-Hot-Temp.jpeg" alt="System after Hair Dryer blowing hot air on the sensor" width="600">  <br> The System after having a Hair Dryer blowing hot air on the Temperature System for a few seconds</td></tr>
</table>

## Running
> After setting up Vivado and exporting hardware
>
> Making a workspace for Vitis and building the platform connected to the .xsa, and an application connected to the platform
>
> The BLE app is built/copied and verified that it opens on an Android or iOs Phone
>
> Hardware of ZedBoard and Peripherials are set up

1. Turn on the ZedBoard
2. Connect to Tera Term (115200 - ZedBoard UART)
3. Run the Vitis application
4. Run BLE App on Phone
5. Connect App to ZedBoard by pressing Connect

## Table of Contents

| File | Description | Status |
|---|---|---|
| [docs/01_hardware_and_vivado.md](docs/01_hardware_and_vivado.md) | Hardware overview, Vivado block design, pin constraints | ✅ |
| [docs/02_vitis_setup.md](docs/02_vitis_setup.md) | Vitis workspace, BSP config, hardware hookup, observed results | ✅ |
| [docs/03_vitis_firmware.md](docs/03_vitis_firmware.md) | Bare-metal C firmware — timer loop, BLE protocol, UART parsing | ✅ |
| [docs/04_ble_protocol.md](docs/04_ble_protocol.md) | Full protocol spec — UUIDs, commands, packet format | ✅ |
| [docs/05_flutter_app.md](docs/05_flutter_app.md) | Flutter app — architecture, state machine, BLE connection flow | ✅ |
| [docs/06_android_setup.md](docs/06_android_setup.md) | Android environment setup and permissions | ✅ |
| [docs/07_ios_setup.md](docs/07_ios_setup.md) | iOS environment setup on macOS — Flutter PATH, Xcode signing | ✅ |
| [docs/bugs_and_fixes.md](docs/bugs_and_fixes.md) | Every bug encountered — symptom, root cause, fix, lesson | ✅ |
| [docs/tool_version_differences.md](docs/tool_version_differences.md) | Vitis 2025.2 and flutter_blue_plus 2.3.10 differences from docs | ✅ |
| [docs/troubleshooting.md](docs/troubleshooting.md) | Quick-reference — error message → cause → fix | ✅ |

---

## System Architecture

```
┌──────────────────────────────────────────────────────┐
│                   ZedBoard (Zynq-7000)               │
│                                                      │
│  ┌──────────┐   AXI-Lite  ┌──────────────────────┐   │
│  │  PS ARM  │────────────►│  Custom OLED AXI IP  │   │
│  │  (main.c)│             │  (PL)                │──►  Onboard OLED
│  │          │             └──────────────────────┘   │
│  │          │    I2C      ┌──────────┐               │
│  │          │◄───────────►│ ADT7420  │ Pmod TMP2     │
│  │          │             └──────────┘               │
│  │          │   UART0     ┌──────────┐               │
│  │          │◄───────────►│  RN4871  │ Pmod BLE      │
│  └──────────┘             └──────────┘               │
└──────────────────────────────────────────────────────┘
                               │ BLE (Transparent UART)
                    ┌──────────▼───────────┐
                    │   Flutter App        │
                    │   Android / iOS      │
                    │                      │
                    │  Scan → Connect →    │
                    │  Subscribe → Stream  │
                    └──────────────────────┘
```

**Data flow:**
1. ADT7420 samples temperature at 1Hz over I2C
2. Firmware computes `TempParts` once — OLED, Tera Term, and BLE all use the same values
3. While streaming, firmware sends `TEMP:23.56C,74.41F\r\n` over UART0 to RN4871
4. RN4871 transmits over BLE Transparent UART Service
5. Flutter app buffers incoming bytes, parses complete `\r\n`-terminated lines, updates display

---

## Development Notes

> Vivado and Vitis projects are developed on Windows and live in the `vivado/` and `vitis/` folders of this repo. The Mac clone of this repo exists solely for iOS Flutter builds — no hardware development happens on the Mac.

---

## Related Projects

This project builds on a series of standalone ZedBoard projects. Work through them in order if starting from scratch — each one validates a component in isolation before it is combined here.

| Project | Description | Link |
|---|---|---|
| ZedBoard OLED | Onboard OLED with custom AXI-Lite IP | [ZedBoard-OLED-Tutorial-Notes](https://github.com/Ava-Kirkland/ZedBoard-OLED-Tutorial-Notes) |
| ZedBoard Pmod TMP2 | ADT7420 temperature sensor over I2C | [ZedBoard-Pmod-TMP2](https://github.com/Ava-Kirkland/Zedboard-Pmod-TMP2) |
| ZedBoard Pmod BLE | RN4871 UART bridge | [ZedBoard-BLE](https://github.com/Ava-Kirkland/ZedBoard-BLE) |
| ZedBoard OLED + TMP2 | Combined OLED display with live temperature | [ZedBoard-OLED-TMP2](https://github.com/Ava-Kirkland/ZedBoard-OLED-TMP2) |
| **ZedBoard BLE OLED TMP2** | **This project — adds Flutter app over BLE** | — |

---

## Key Tips — Read Before Starting

These are the things that cost the most time to find. Check them before you assume something is broken.

1. **`standalone_stdout` defaults to UART0 in Vitis — change it once in the BSP settings.**  
   Set both `standalone_stdin` and `standalone_stdout` to `ps7_uart_1`. This is a one-time change. Forgetting it routes `xil_printf` to the BLE module instead of Tera Term.

2. **The RN4871 does not include its service UUID in its advertisement payload.**  
   UUID-based BLE scan filtering won't find it. Filter by device name (`RN4870` or `RN4871`). This is hardware-specific — most BLE peripherals do advertise their UUID.

3. **iOS BLE redirecting to Settings on every Connect tap is caused by missing `pod install`, not a permissions bug.**  
   Run `pod install` in `ios/` before the first iOS build. Without it, the BLE native libraries are not linked and the app misbehaves in ways that look like permission issues.

4. **iOS BLE will not work without running `pod install` on the Mac first.**  
   Flutter does not do this automatically. Run it once from `flutter/ble_temp_app/ios/` before your first iOS build. Symptoms look like BLE stack issues, not a missing dependency.

5. **Always open `Runner.xcworkspace`, never `Runner.xcodeproj` in Xcode.**  
   CocoaPods requires the workspace. The wrong file opens fine but BLE libraries won't be linked.

6. **CMD mode (`$$$`) on UART1 must gate all UART0 output.**  
   If UART0 is active during CMD mode, RN4871 interprets the output as commands and responds with `Err` spam. The firmware sets `cmd_mode = true` and stops all UART0 writes until `---` is received on UART1.

7. **`TempParts` must be computed once per cycle and shared across all output paths.**  
   Calling `celsius_to_fahrenheit()` separately in each output path (OLED vs. BLE) produces a 0.01°F discrepancy due to floating-point rounding. Compute once, pass the struct everywhere.

8. **`device.connect()` requires `license: License.nonprofit` in flutter_blue_plus 2.3.10.**  
   The parameter is not in older docs. Omitting it causes a compile error that looks unrelated.

---

## Possible Version 2 Changes

- C/F toggle — tap to switch primary display unit
- App icon and display name
- ProGuard rule for Android release build
- Auto-reconnect with exponential backoff

---

## External References

- [flutter_blue_plus pub.dev](https://pub.dev/packages/flutter_blue_plus)
- [RN4871 User Guide (Microchip)](https://www.microchip.com/en-us/product/RN4871)
- [ADT7420 Datasheet (Analog Devices)](https://www.analog.com/en/products/adt7420.html)
- [Android Bluetooth Permissions (Android 12+)](https://developer.android.com/about/versions/12/features/bluetooth-permissions)
