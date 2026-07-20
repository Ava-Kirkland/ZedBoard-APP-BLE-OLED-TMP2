/*
ble_uart.c

Driver for initializing the uarts for a Pmod BLE project
UART 0 - ZedBoard <-> Pmod BLE
UART 1 - ZedBoard <-> PC
*/

#include "ble_uart.h"


// UART data structure instances
XUartPs Uart0;   // BLE UART
XUartPs Uart1;   // Terminal UART

void clear_uart(){
    //Flush UART 0 RX buffer
    while(XUartPs_IsReceiveData(Uart0.Config.BaseAddress)){
        u8 garabage;
        XUartPs_Recv(&Uart0, &garabage, 1);
    }

    //Flush UART 1 RX buffer
    while(XUartPs_IsReceiveData(Uart1.Config.BaseAddress)){
        u8 garabage;
        XUartPs_Recv(&Uart1, &garabage, 1);
    }
    // 1 second of quiet
    sleep(1);
}


int init_uart()
{
    XUartPs_Config *cfg;
    int status;

    // -------------------------
    // Initialize UART1 (Terminal)
    // -------------------------

    //Does the device have a UART structure
    cfg = XUartPs_LookupConfig(XPAR_XUARTPS_1_BASEADDR);
    if (cfg == NULL) {
        xil_printf("UART1 LookupConfig FAILED\r\n");
        return XST_FAILURE;
    }
    //Link the UART structure of the device to the UART struct instance
    status = XUartPs_CfgInitialize(&Uart1, cfg, cfg->BaseAddress);
    if (status != XST_SUCCESS) {
        xil_printf("UART1 CfgInitialize FAILED\r\n");
        return status;
    }
    //Set Baud Rate
    XUartPs_SetBaudRate(&Uart1, 115200);
    xil_printf("UART1 initialized (Terminal UART) @115200\r\n");

    // -------------------------
    // Initialize UART0 (BLE)
    // -------------------------
    cfg = XUartPs_LookupConfig(XPAR_XUARTPS_0_BASEADDR);
    if (cfg == NULL) {
        xil_printf("UART0 LookupConfig FAILED\r\n");
        return XST_FAILURE;
    }

    status = XUartPs_CfgInitialize(&Uart0, cfg, cfg->BaseAddress);
    if (status != XST_SUCCESS) {
        xil_printf("UART0 CfgInitialize FAILED\r\n");
        return status;
    }

    XUartPs_SetBaudRate(&Uart0, 115200);
    xil_printf("UART0 initialized (BLE UART) @115200\r\n");


    xil_printf("Both UARTs ready.\r\n\r\n");
    //clear_uart();
    return XST_SUCCESS;
}
