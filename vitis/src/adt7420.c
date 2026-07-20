/**
 * adt7420.c
 * Driver implementation for the ADT7420 I2C temperature sensor
 * Datasheet: https://www.analog.com/media/en/technical-documentation/data-sheets/ADT7420.pdf
 */

#include "adt7420.h"

int ADT7420_Init(XIicPs *Iic, u32 baseAddress) {
    XIicPs_Config *Config;
    int Status;

    Config = XIicPs_LookupConfig(baseAddress);
    if (Config == NULL) {
        xil_printf("ADT7420: I2C LookupConfig failed\r\n");
        return XST_FAILURE;
    }

    Status = XIicPs_CfgInitialize(Iic, Config, Config->BaseAddress);
    if (Status != XST_SUCCESS) {
        xil_printf("ADT7420: I2C CfgInitialize failed: %d\r\n", Status);
        return XST_FAILURE;
    }

    Status = XIicPs_SetSClk(Iic, 100000); // 100 kHz
    if (Status != XST_SUCCESS) {
        xil_printf("ADT7420: I2C set clock failed\r\n");
        return XST_FAILURE;
    }

    xil_printf("ADT7420: I2C initialized successfully\r\n");
    return XST_SUCCESS;
}

float ADT7420_ReadTemperature(XIicPs *Iic) {
    u8 reg = ADT7420_REG_TEMP_MSB;
    u8 data[2] = {0};
    int Status;
    int timeout;

    // Set sensor register pointer to temperature MSB
    Status = XIicPs_MasterSendPolled(Iic, &reg, 1, ADT7420_I2C_ADDR);
    if (Status != XST_SUCCESS) {
        xil_printf("ADT7420: send failed: %d\r\n", Status);
        return ADT7420_ERR_SEND;
    }

    timeout = ADT7420_IIC_TIMEOUT;
    while (XIicPs_BusIsBusy(Iic) && timeout > 0) { timeout--; }
    if (timeout <= 0) return ADT7420_ERR_TIMEOUT;

    // Read MSB and LSB — sensor auto-increments register after MSB
    Status = XIicPs_MasterRecvPolled(Iic, data, 2, ADT7420_I2C_ADDR);
    if (Status != XST_SUCCESS) {
        xil_printf("ADT7420: recv failed: %d\r\n", Status);
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

TempParts ADT7420_DecomposeTemp(float t) {
    TempParts parts;
    parts.sign  = (t < 0) ? -1 : 1;
    float abs_t = (t < 0) ? -t : t;
    parts.whole = (int)abs_t;
    parts.frac  = (int)((abs_t - parts.whole) * 100);
    return parts;
}

void ADT7420_Print_Temp(float t) {
    if (t <= ADT7420_SENTINEL_THRESHOLD) {
        xil_printf("ADT7420: read error (code: %d) - check pull-ups, address, wiring\r\n", (int)t);
        return;
    }

    TempParts c = ADT7420_DecomposeTemp(t);
    TempParts f = ADT7420_DecomposeTemp(celsius_to_fahrenheit(t));

    xil_printf("Temperature: %s%d.%02dC\t %s%d.%02dF\r\n",
               (c.sign < 0 ? "-" : ""), c.whole, c.frac,
               (f.sign < 0 ? "-" : ""), f.whole, f.frac);
}

void ADT7420_Print_Temp_Parts(const TempParts *c, const TempParts *f){
    xil_printf("Temperature: %s%d.%02dC\t %s%d.%02dF\r\n",
               (c->sign < 0 ? "-" : ""), c->whole, c->frac,
               (f->sign < 0 ? "-" : ""), f->whole, f->frac);
}
