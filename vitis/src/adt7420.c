/**
 * adt7420.c
 * Driver implementation for the ADT7420 I2C temperature sensor
 * Datasheet: https://www.analog.com/media/en/technical-documentation/data-sheets/ADT7420.pdf
 */

#include "adt7420.h"

int ADT7420_Init(XIicPs *Iic, u32 baseAddress) {
    XIicPs_Config *Config;
    int status;

    Config = XIicPs_LookupConfig(baseAddress);
    if (Config == NULL) {
        xil_printf("ADT7420: I2C LookupConfig failed\r\n");
        return XST_FAILURE;
    }

    status = XIicPs_CfgInitialize(Iic, Config, Config->BaseAddress);
    if (status != XST_SUCCESS) {
        xil_printf("ADT7420: I2C CfgInitialize failed: %d\r\n", status);
        return XST_FAILURE;
    }

    status = XIicPs_SetSClk(Iic, 100000); // 100 kHz
    if (status != XST_SUCCESS) {
        xil_printf("ADT7420: I2C set clock failed\r\n");
        return XST_FAILURE;
    }

    xil_printf("ADT7420: I2C initialized successfully\r\n");
    return XST_SUCCESS;
}

float ADT7420_ReadTemperature(XIicPs *Iic) {
    u8 reg = ADT7420_REG_TEMP_MSB;
    u8 data[2] = {0};
    int status;
    int timeout;

    // Set sensor register pointer to temperature MSB
    status = XIicPs_MasterSendPolled(Iic, &reg, 1, ADT7420_I2C_ADDR);
    if (status != XST_SUCCESS) {
        xil_printf("ADT7420: send failed: %d\r\n", status);
        return ADT7420_ERR_SEND;
    }

    timeout = ADT7420_IIC_TIMEOUT;
    while (XIicPs_BusIsBusy(Iic) && timeout > 0) { timeout--; }
    if (timeout <= 0) return ADT7420_ERR_TIMEOUT;

    // Read MSB and LSB — sensor auto-increments register after MSB
    status = XIicPs_MasterRecvPolled(Iic, data, 2, ADT7420_I2C_ADDR);
    if (status != XST_SUCCESS) {
        xil_printf("ADT7420: recv failed: %d\r\n", status);
        return ADT7420_ERR_RECV;
    }

    timeout = ADT7420_IIC_TIMEOUT;
    while (XIicPs_BusIsBusy(Iic) && timeout > 0) { timeout--; }
    if (timeout <= 0) return ADT7420_ERR_TIMEOUT;

    // Combine MSB and LSB, then strip lower 3 bits (unused in 13-bit mode)
    s16 raw = (s16)((data[0] << 8) | data[1]);
    raw >>= 3;

    // 13-bit mode LSB resolution = 0.0625C
    return raw * 0.0625f;
}

TempParts ADT7420_DecomposeTemp(float temp) {
    TempParts parts;
    parts.sign  = (temp < 0) ? -1 : 1;
    float abs_t = (temp < 0) ? -temp : temp;
    parts.whole = (int)abs_t;
    parts.frac  = (int)((abs_t - parts.whole) * 100);
    return parts;
}

void ADT7420_PrintTemp(float temp) {
    if (temp <= ADT7420_SENTINEL_THRESHOLD) {
        xil_printf("ADT7420: read error (code: %d) - check pull-ups, address, wiring\r\n", (int)temp);
        return;
    }

    TempParts temp_c = ADT7420_DecomposeTemp(temp);
    TempParts temp_f = ADT7420_DecomposeTemp(celsius_to_fahrenheit(temp));

    xil_printf("Temperature: %s%d.%02dC\t %s%d.%02dF\r\n",
               (temp_c.sign < 0 ? "-" : ""), temp_c.whole, temp_c.frac,
               (temp_f.sign < 0 ? "-" : ""), temp_f.whole, temp_f.frac);
}

void ADT7420_PrintTempParts(const TempParts *temp_c, const TempParts *temp_f){
    xil_printf("Temperature: %s%d.%02dC\t %s%d.%02dF\r\n",
               (temp_c->sign < 0 ? "-" : ""), temp_c->whole, temp_c->frac,
               (temp_f->sign < 0 ? "-" : ""), temp_f->whole, temp_f->frac);
}
