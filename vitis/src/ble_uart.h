/*
ble_uart.h

* Driver for creating te uarts for the termainal(Uart 1) and for ZedBoard <-> Pmod BLE (Uart 0)

*/

#ifndef BLE_UART_H
#define BLE_UART_H



#include <stdio.h>
#include <xstatus.h>
#include <xuartps_hw.h>
#include "xuartps.h"
#include "xparameters.h"
#include "xil_printf.h"
#include "sleep.h"

// UART data structure instances 
extern  XUartPs Uart0;   // BLE UART
extern XUartPs Uart1;   // Terminal UART

// extern declares not defines, so it is a promise that it will be defined later

void clear_uart();


int init_uart();


#endif /*BLE_UART_H*/