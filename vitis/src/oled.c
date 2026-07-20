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

void initOled(oledControl *myOled, u32 baseAddress) {
    myOled->baseAddress = baseAddress;
}

void writeCharOled(oledControl *myOled, char myChar) {
    u32 status = 0;

    Xil_Out32(myOled->baseAddress + 8, myChar); // load character into data register
    Xil_Out32(myOled->baseAddress, 0x1);        // trigger send

    while (!status) {
        status = Xil_In32(myOled->baseAddress + 4); // poll status until done
    }

    Xil_Out32(myOled->baseAddress + 4, 0x0);    // clear status register
}

void printOled(oledControl *myOled, const char *myString) {
    while (*myString != 0) {
        writeCharOled(myOled, *myString);
        myString++;
    }
}

void clearOled(oledControl *myOled) {
    // 4 pages x 16 characters = 64 total slots
    u32 i;
    for (i = 0; i < 64; i++) {
        writeCharOled(myOled, ' ');
    }
}

void oled_print_line(oledControl *myOled, const char *str) {
    char buf[OLED_LINE_LEN + 1];              // +1 for null terminator
    snprintf(buf, sizeof(buf), "%-16s", str); // left-align, space-pad to 16 chars
    printOled(myOled, buf);
}
