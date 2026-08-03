# CHANGELOG

## [v2.1] — 2026-08-03
 
### Fixed
 
- **OLED IP updated to `oledAddition_v4.0`** — AXI HDL now guards register writes against prior operation completion per register type (write vs. power command). Resolves OLED display corruption (wrong rows, blank display, erratic updates every second) when additional AXI peripherals are present in the Vivado system. `oled.c` updated with matching software-side guards. See [ZedBoard-OLED-Addition](https://github.com/Ava-Kirkland/ZedBoard-OLED-Addition) for full IP change details. *(Bug #16)*
- **BLE receive buffer OOB fix (`main.c`)** — Tightened `buf_index > 0` to `buf_index >= 2` in the `\r\n` terminator check, preventing a negative array index (`buf[-1]`) when `'\n'` arrives as the first byte in the buffer. Previously caused program termination on disconnect. *(Bug #17)*
### Changed
 
- `vivado/` — `oledAddition_v3.0` replaced with `oledAddition_v4.0`
- `vitis/src/oled.c` — software-side operation completion guards added to match `v4.0` IP behavior
- `vitis/src/main.c` — BLE parser buffer index guard corrected (`buf_index > 0` → `buf_index >= 2`)

---

## [v2.0]

### Changed

- Changed OLED IP from the tutorial version to oledAddition_v3.0
- Reads temperature ~200ms and after 5 valid readings, average is sent to the peripherials: ~ 1Hz
    - Smoother Temperature Readings on Peripherials
- `ip\`
- `main.c` non-blocking temperature readings to average 5 valid readings and sent average to peripherials