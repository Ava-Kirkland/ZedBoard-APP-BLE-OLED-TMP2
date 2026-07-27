/**
 * oled.c
 * Driver implementation for the ZedBoard on-board OLED display
 * Communication is via AXI memory-mapped registers:
 *   offset 0x00 — control register (write 0x1 to trigger send)
 *   offset 0x04 — status register  (poll until non-zero, then clear)
 *   offset 0x08 — data register    (write character before triggering)
 */

#include "oled.h"
#include <xil_io.h>
#include <stdio.h>

void OLED_Init(OLED_Control_t *my_oled, u32 baseAddress) {
    my_oled->base_address = baseAddress;
}

void OLED_WriteChar(OLED_Control_t *my_oled, char ch) {
    u32 status = 0;

    Xil_Out32(my_oled->base_address + 8, ch); // load character into data register
    Xil_Out32(my_oled->base_address, 0x1);        // trigger send

    while (!status) {
        status = Xil_In32(my_oled->base_address + 4); // poll status until done
    }

    Xil_Out32(my_oled->base_address + 4, 0x0);    // clear status register
}

void OLED_Print(OLED_Control_t *my_oled, const char *str) {
    while (*str != 0) {
        OLED_WriteChar(my_oled, *str);
        str++;
    }
}

void OLED_Clear(OLED_Control_t *my_oled) {
    // 4 pages x 16 characters = 64 total slots
    u32 i;
    for (i = 0; i < 64; i++) {
        OLED_WriteChar(my_oled, ' ');
    }
}

void OLED_PrintLine(OLED_Control_t *my_oled, const char *str) {
    char buf[OLED_LINE_LEN + 1];              // +1 for null terminator
    snprintf(buf, sizeof(buf), "%-16s", str); // left-align, space-pad to 16 chars
    OLED_Print(my_oled, buf);
}

void OLED_RepowerOn(OLED_Control_t *my_oled){
    u32 cmd = 1; // set in software and will be cleared in hardware when the command's sequence is over

    Xil_Out32(my_oled->base_address+ 12, 0x2); // Send a 1 to reg3[1] bit

    //poll untill hardware clears the command as it finishs
    while(cmd){
        cmd = Xil_In32(my_oled->base_address + 12);
    }
    
}

void OLED_Off(OLED_Control_t *my_oled){
    u32 cmd = 1; // set in software and will be cleared in hardware when the command's sequence is over

    Xil_Out32(my_oled->base_address+ 12, 0x1); // Send a 1 to reg3[0] bit

    //poll untill hardware clears the command as it finishs
    while(cmd){
        cmd = Xil_In32(my_oled->base_address + 12);
    }
    
}
