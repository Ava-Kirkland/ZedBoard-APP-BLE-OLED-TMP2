/**
 * oled.h
 * Driver for the ZedBoard on-board OLED display (4 pages x 16 characters)
 * Provides character, string, line, and clear operations via AXI memory-mapped interface
 */

#ifndef OLED_H
#define OLED_H

#include <xil_types.h>

// Number of characters per OLED page line
#define OLED_LINE_LEN 16

typedef struct oledControl {
    u32 baseAddress;
} oledControl;

// Initialize the OLED controller with a given AXI base address
void initOled(oledControl *myOled, u32 baseAddress);

// Write a single character to the OLED at the current cursor position
void writeCharOled(oledControl *myOled, char myChar);

// Write a null-terminated string to the OLED — no padding applied
// Use oled_print_line for full-line writes to avoid stale characters
void printOled(oledControl *myOled, const char *myString);

// Clear the OLED by overwriting all 64 character slots with spaces
void clearOled(oledControl *myOled);

// Write exactly 16 characters to the OLED — left-aligned, space-padded
// Preferred over printOled when writing full lines
void oled_print_line(oledControl *myOled, const char *str);

#endif /* OLED_H */
