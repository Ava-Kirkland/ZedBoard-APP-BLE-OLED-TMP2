/**
 * adt7420.h
 * Driver for the ADT7420 I2C temperature sensor (Pmod TMP2)
 * Operates in default 13-bit mode — resolution of 0.0625C per LSB
 * Sensor accuracy: +/-0.25C typical, +/-0.5C max (per datasheet)
 */

#ifndef ADT7420_H
#define ADT7420_H

#include "xparameters.h"
#include "xiicps.h"
#include "xil_printf.h"
#include "sleep.h"
#include <xstatus.h>

// I2C address — JP1=JP2 open on Pmod TMP2
#define ADT7420_I2C_ADDR        0x4B

// Register map
#define ADT7420_REG_TEMP_MSB    0x00
#define ADT7420_REG_CONFIG      0x03

// Timeout count for I2C bus busy polling
#define ADT7420_IIC_TIMEOUT     100000

// Sentinel values returned on read failure — all below ADT7420_SENTINEL_THRESHOLD
// which is physically impossible (sensor minimum is -40C)
#define ADT7420_ERR_SEND        -999.0f  // XIicPs_MasterSendPolled failed
#define ADT7420_ERR_RECV        -998.0f  // XIicPs_MasterRecvPolled failed
#define ADT7420_ERR_TIMEOUT     -997.0f  // I2C bus busy timeout exceeded

// Threshold for detecting sentinel error values vs valid temperature readings
// Safe because the ADT7420 cannot physically read below -40C
#define ADT7420_SENTINEL_THRESHOLD  -500.0f

// Holds a temperature float decomposed into printable integer parts
// Used to avoid float formatting which is unavailable in xil_printf
typedef struct {
    int sign;   // -1 if negative, 1 if positive
    int whole;  // integer part of the absolute value
    int frac;   // fractional part as two decimal digits (0-99)
} TempParts;

// Convert Celsius to Fahrenheit
static inline float celsius_to_fahrenheit(float c) {
    return (c * 9.0f / 5.0f) + 32.0f;
}

// Initialize the I2C controller and configure clock to 100kHz
// Returns XST_SUCCESS or XST_FAILURE
int ADT7420_Init(XIicPs *Iic, u32 baseAddress);

// Read temperature from sensor and return as float in Celsius
// Returns a sentinel value (<= ADT7420_SENTINEL_THRESHOLD) on any error
float ADT7420_ReadTemperature(XIicPs *Iic);

// Decompose a float temperature into sign, whole, and fractional integer parts
// Use this for both Celsius and Fahrenheit to avoid repeating split logic
TempParts ADT7420_DecomposeTemp(float t);

// Format and print a temperature reading to UART (Celsius and Fahrenheit)
// Accepts a pre-read float to avoid a redundant sensor read
void ADT7420_Print_Temp(float t);

// Print TempParts on the Terminal
void ADT7420_Print_Temp_Parts(const TempParts *c, const TempParts *f);
#endif /* ADT7420_H */
