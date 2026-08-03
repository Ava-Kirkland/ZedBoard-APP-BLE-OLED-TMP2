/**
 * oled.c
 * Driver implementation for the ZedBoard on-board OLED display
 * Communication is via AXI memory-mapped registers:
 *   offset 0x00 — control register (write 0x1 to trigger send)
 *   offset 0x04 — status register  (poll until non-zero, then clear)
 *   offset 0x08 — data register    (write character before triggering)
 *   offset 0x0C — power off/on     (write 0x1 to turn off, write 0x2 to turn on)
 */

#include "oled.h"
#include <xil_io.h>
#include <stdio.h>

void OLED_Init(OLED_Control_t *my_oled, u32 baseAddress) {
    my_oled->base_address = baseAddress;
}

void OLED_WriteChar(OLED_Control_t *my_oled, char ch) {
    u32 status = 0;

    //// Guard: poll slv_reg0[0] until HW clears it — confirms FSM consumed previous command and is in DONE
    while(Xil_In32(my_oled->base_address) & 0x1);

    Xil_Out32(my_oled->base_address + 8, ch); // load character into data register
    Xil_Out32(my_oled->base_address, 0x1);        // trigger send

    while (!status) {
        status = (Xil_In32(my_oled->base_address + 4) & 0x1); // poll bit 0 of status until set
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
    u32 cmd = 1; 
    
    //Guard: wait for HW to clear bit 1 (is in DONE state) before issuing a new power-on command
    while(Xil_In32(my_oled->base_address +12) & 0x2);
    
    //Set in SW and cleared in Hardware
    Xil_Out32(my_oled->base_address+ 12, 0x2); // Set slv_reg3[1] = 1

    //poll until hardware clears bit 1 - confirms power-on sequence complete
    while(cmd){
        cmd = (Xil_In32(my_oled->base_address + 12) & 0x2);
    }
    
}

void OLED_Off(OLED_Control_t *my_oled){
    u32 cmd = 1; 

    //Guard: wait for HW to clear bit 0 before starting a new power-off command
    while(Xil_In32(my_oled->base_address + 12) & 0x1);

    //Set in SW and clear in HW
    Xil_Out32(my_oled->base_address+ 12, 0x1); // Send reg3[0] = 1

    //Poll until HW clears bit 0 - confirms power-off sequence complete
    while(cmd){
        cmd = (Xil_In32(my_oled->base_address + 12) & 0x1);
    }
    
}
