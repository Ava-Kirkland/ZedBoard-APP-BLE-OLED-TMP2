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
    u32 base_address;
} OLED_Control_t;

// Initialize the OLED controller with a given AXI base address
void OLED_Init(OLED_Control_t *my_oled, u32 base_address);

// Write a single character to the OLED at the current cursor position
void OLED_WriteChar(OLED_Control_t *my_oled, char ch);

// Write a null-terminated string to the OLED — no padding applied
// Use oled_print_line for full-line writes to avoid stale characters
void OLED_Print(OLED_Control_t *my_oled, const char *str);

// Clear the OLED by overwriting all 64 character slots with spaces
void OLED_Clear(OLED_Control_t *my_oled);

// Write exactly 16 characters to the OLED — left-aligned, space-padded
// Preferred over printOled when writing full lines
void OLED_PrintLine(OLED_Control_t *my_oled, const char *str);


//For turning on the OLED again because it will turn on at the start of the program automatically
void OLED_RepowerOn(OLED_Control_t *my_oled);



void OLED_Off(OLED_Control_t *my_oled);

#endif /* OLED_H */
