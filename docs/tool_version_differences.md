# Tool Version Differences

`Project: APP-BLE-OLED-TMP2 | Vitis 2025.2 | flutter_blue_plus 2.3.10`

This file documents every place where the current toolchain behavior differs from tutorials, older documentation, or expected defaults. Each entry is marked **confirmed working** or **hypothesis** depending on whether the fix was validated on hardware.

---

## 1. `xiltimer.h` replaces `xtime_l.h` for time functions

**Tutorial/docs behavior:** Use `#include "xtime_l.h"` and `XTime_GetTime()` from that header.

**2025.2 behavior:** `xtime_l.h` still exists but `xiltimer.h` is the current API. `COUNTS_PER_SECOND` is defined in `xtimer_config.h` (included transitively via `xiltimer.h`) as `XPAR_CPU_CORE_CLOCK_FREQ_HZ / 2`.

**Impact:** Build will succeed with either header, but `xiltimer.h` is the correct include for 2025.2.

**Fix:** `#include "xiltimer.h"` and `#include "xtimer_config.h"`. Use `COUNTS_PER_SECOND` directly for timer comparisons. **Confirmed working.**

---

## 2. `XUartPs_LookupConfig` takes a base address, not a device ID

**Tutorial/docs behavior:** `XUartPs_LookupConfig(XPAR_XUARTPS_0_DEVICE_ID)` — pass the device ID macro.

**2025.2 behavior:** `XUartPs_LookupConfig` takes a base address: `XUartPs_LookupConfig(XPAR_XUARTPS_0_BASEADDR)`. Passing `DEVICE_ID` compiles but produces incorrect or NULL config lookups at runtime.

**Impact:** Passing `DEVICE_ID` causes UART init to silently fail or return a wrong config, resulting in no UART output.

**Fix:** Use `XPAR_XUARTPS_0_BASEADDR` and `XPAR_XUARTPS_1_BASEADDR`. **Confirmed working.**

---

## 3. `XIicPs_LookupConfig` takes a base address, not a device ID

**Tutorial/docs behavior:** `XIicPs_LookupConfig(XPAR_XIICPS_0_DEVICE_ID)`.

**2025.2 behavior:** Same pattern as UART — pass `XPAR_XIICPS_0_BASEADDR`.

**Impact:** Passing `DEVICE_ID` causes I2C init to fail silently.

**Fix:** Use `XPAR_XIICPS_0_BASEADDR`. **Confirmed working.**

---

## 4. Custom IP driver files must be manually copied to `src/`

**Tutorial/docs behavior:** Vivado-generated custom IP includes a driver that Vitis discovers and links automatically via the TCL driver loading mechanism.

**2025.2 behavior:** The TCL driver loading mechanism for custom IP is broken in Vitis 2025.x. Driver files are not automatically included in the build.

**Impact:** If driver `.c`/`.h` files are not manually copied into the application `src/` directory, the build fails with missing symbol errors, or succeeds but with stub implementations.

**Fix:** Copy `oled.c`, `oled.h`, `adt7420.c`, `adt7420.h`, `ble_uart.c`, `ble_uart.h`, `temp_display.c`, `temp_display.h` directly into `vitis/src/` alongside `main.c`. **Confirmed working. This is a known temporary workaround, not the correct long-term solution.**

---

## 5. `standalone_stdout` defaults to `ps7_uart_0` (BLE UART)

**Tutorial/docs behavior:** Documentation assumes `xil_printf` routes to the USB-UART terminal by default.

**2025.2 behavior:** Vitis initializes `standalone_stdout` and `standalone_stdin` to `ps7_uart_0` (UART0). When UART0 is used for BLE, all `xil_printf` output goes to the RN4871.

**Impact:** No Tera Term output. RN4871 receives firmware debug text as BLE commands and responds with `Err` spam.

**Fix:** In BSP settings, set both `standalone_stdin` and `standalone_stdout` to `ps7_uart_1` in both BSP nodes. One-time change per workspace. **Confirmed working.**

---

## 6. Vivado 2025 appends `_0` suffix to external block design ports

**Tutorial/docs behavior:** External ports created from block design pins take the signal name exactly.

**2025.2 behavior:** Vivado 2025 appends `_0` to the port name when making a pin external (e.g., `oled_vdd` becomes `oled_vdd_0`). The XDC constraints file uses the original names — synthesis will not map correctly if the names differ.

**Impact:** Pin assignment errors or DRC failures during implementation.

**Fix:** After making pins external, manually rename each port in the block design to remove the `_0` suffix. **Confirmed working.**

---

## 7. `oledAddition_v3.0` base address macro differs from original OLED IP

**Tutorial/docs behavior:** Original OLED IP tutorial uses the macro generated for the original IP name.

**2025.2 behavior:** When `oledAddition_v3.0` is added to the block design, Vivado generates `XPAR_OLEDADDITION_0_BASEADDR` as the base address macro (not the original tutorial IP's macro name).

**Impact:** Using the wrong macro compiles silently but the driver writes to the wrong AXI address, producing no OLED output.

**Fix:** Use `XPAR_OLEDADDITION_0_BASEADDR` in all software that references the OLED base address. **Confirmed working.**

---

## 8. `device.connect()` requires `license: License.nonprofit` in flutter_blue_plus 2.3.10

**Tutorial/docs behavior:** Older flutter_blue_plus docs show `device.connect()` with no license parameter.

**2.3.10 behavior:** `License.nonprofit` parameter is required. Omitting it produces a compile error that does not clearly indicate the missing parameter.

**Impact:** App fails to build.

**Fix:** `await device.connect(license: License.nonprofit)`. **Confirmed working.**

---

## 9. Board Initialization must be set to FSBL in Vitis for custom IP

**Tutorial/docs behavior:** Not always mentioned in tutorials for simple projects.

**2025.2 behavior:** Custom IP applications require Board Initialization set to FSBL. Without it, the PS-PL interface may not be correctly initialized and the AXI IP will be unresponsive.

**Impact:** OLED produces no output despite correct software and bitstream.

**Fix:** In Vitis application settings, set Board Initialization to FSBL. **Confirmed working.**