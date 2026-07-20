/**
 * main.c
 * ZedBoard + Custom BLE APP + Pmod BLE + ADT7420 Temperature Sensor + OLED Display
 * Reads temperature via I2C every second and displays
 * Celsius and Fahrenheit on the on-board OLED and APP (if connected)
 */

#include "oled.h"
#include "adt7420.h"
#include "temp_display.h"
#include <stdio.h>
#include <xil_printf.h>
#include <xparameters.h>
#include "sleep.h"
#include "ble_uart.h"
#include <stdbool.h>
#include <xiltimer.h>
#include <xtimer_config.h>
#include <string.h>
#include <xuartps.h>

#define I2C_BASEADDR XPAR_XIICPS_0_BASEADDR
#define START "START_TEMP"
#define STOP "STOP_TEMP"
#define STREAM "%STREAM_OPEN%"
#define DISCONNECT "%DISCONNECT%"
#define TEMP_LINE_LEN 32

XIicPs Iic;
XTime last_sample;
XTime now;

void parse_command(char *buf, bool *connected, bool *streaming);

int main() {
    oledControl myOled;
    //added
    u8 newline[2] = {'\r', '\n'};
    int status;
    float temp;
    u8 c;
    u32 received;
    char buf[64] = {0};
    bool streaming = false;
    bool connected = false;
    bool cmd_mode = false;
    u32 buf_index =0;
    char temp_line[TEMP_LINE_LEN + 1];
    int dollar_count = 0;
    int dash_count = 0;

    // Init OLED first — confirms display is alive before I2C init runs
    initOled(&myOled, XPAR_OLEDCONTROLP4_0_BASEADDR);

//added

    status = init_uart();
    if (status != XST_SUCCESS){
        xil_printf("ERROR: UART init");
        return XST_FAILURE;
    }
    // needs to be after init_uart so the sleeper timer is initialized before gets to this line

    
    oled_print_line(&myOled, "=== TEMP SENSOR=");
    oled_print_line(&myOled, "                ");
    oled_print_line(&myOled, "Initializing... ");
    oled_print_line(&myOled, "                ");
    sleep(2); // hold splash long enough to be visible

    xil_printf("=== ADT7420 + OLED Integration ===\r\n");

    if (ADT7420_Init(&Iic, I2C_BASEADDR) != XST_SUCCESS) {
        xil_printf("ADT7420: init failed — halting\r\n");
        oled_print_line(&myOled, "=== TEMP SENSOR=");
        oled_print_line(&myOled, "                ");
        oled_print_line(&myOled, "I2C INIT FAILED ");
        oled_print_line(&myOled, "CHECK HARDWARE  ");
        return -1;
    }

    XTime_GetTime(&last_sample); // void function, writes into your variable via pointer

    while (1) {

        if(!cmd_mode){
            XTime_GetTime(&now);
            if((now - last_sample) >= COUNTS_PER_SECOND){
                //update OLED
                temp = ADT7420_ReadTemperature(&Iic); // single read per cycle
                if (temp <= ADT7420_SENTINEL_THRESHOLD) {
                    xil_printf("ADT7420: read error (code: %d) - check pull-ups, address, wiring\r\n", (int)temp);
                    if (streaming) {
        XUartPs_Send(&Uart0, (u8*)"ERROR:SENSOR_FAIL\r\n", 19);
                    } 
                } else{
                TempParts c = ADT7420_DecomposeTemp(temp);
                TempParts f = ADT7420_DecomposeTemp(celsius_to_fahrenheit(temp));
                ADT7420_Print_Temp_Parts(&c, &f);                   // UART output
                oled_display_temp_parts(&myOled, &c, &f);     
                      // OLED output

                if (streaming && !cmd_mode) {

                    snprintf(temp_line, sizeof(temp_line), "TEMP:%s%d.%02dC,%s%d.%02dF\r\n", 
                        (c.sign < 0 ? "-" : ""), c.whole, c.frac, 
                        (f.sign < 0 ? "-" : ""), f.whole, f.frac);
                    XUartPs_Send(&Uart0, (u8*)temp_line, strlen(temp_line));
                }
                }
                last_sample = now;
            }
            
        }

        // Terminal → BLE
        if (XUartPs_IsReceiveData(Uart1.Config.BaseAddress)) {

            //Receives 1 character
            received = XUartPs_Recv(&Uart1, &c, 1);
        if (received == 1){

            if ( c == '\r'){
    
                XUartPs_Send(&Uart1, newline, 2);
            }else{
                //Echo Terminal                
                XUartPs_Send(&Uart1, &c, 1);                       
            }
                //U0 - BLE
                XUartPs_Send(&Uart0, &c, 1);
            
            //$$$ command doesn't have a \n so can't put it in te parse_command function
            if(c == '$'){
                dollar_count++;
                if(dollar_count >=3){
                    cmd_mode = true;
                    dollar_count = 0;
                }
                
            }else {
                dollar_count = 0;
            }
            if (cmd_mode && c == '-'){
                dash_count++;
                if(dash_count >= 3){
                    cmd_mode = false;
                    dash_count = 0;
                    buf_index = 0;
                    XTime_GetTime(&last_sample);
                }
                
        
            }else{
                    dash_count = 0;
            }
        }
    }

    if (XUartPs_IsReceiveData(Uart0.Config.BaseAddress)) {
        
            received = XUartPs_Recv(&Uart0, &c, 1);
            
        if (received == 1){
            XUartPs_Send(&Uart1, &c, 1); // always send to Terminal

        
            
         if (!cmd_mode){
            if (buf_index >= 63){
                buf_index =0;
            }
            
            buf[buf_index] = c;
            buf_index++;
            if( c == '\n' && buf_index > 0 && buf[buf_index -2] == '\r'){
                buf[buf_index -2] = '\0';
                parse_command(buf, &connected, &streaming);
                buf_index =0;
            }
            else if (buf_index > 1 && c == '%' && buf[0] == '%'){
                buf[buf_index] = '\0';
                parse_command(buf, &connected, &streaming);
                buf_index = 0;
            }
        }
        }
    }
    }
    return 0;
}


void parse_command(char *buf, bool *connected, bool *streaming){
    //Line below is for debugfing what is in the buffer
    //xil_printf("DEBUG parse: [%s] first=%d\r\n", buf, (int)buf[0]);
    switch(buf[0]){
        case '%':
            if (strstr(buf, STREAM) != NULL){
                *connected = true;
            } else if (strstr(buf, DISCONNECT) != NULL){
                *connected = false;
                *streaming = false;
            }
            break;
        case 'S':
            if(strcmp(buf,START) == 0){
                if(*connected) *streaming = true;
            } else if (strcmp(buf, STOP) == 0){
                *streaming = false;
            }
            break;
        default:
            XUartPs_Send(&Uart0, (u8*)"ERROR:UNKNOWN_CMD\r\n", 19);
            break;
    }
}
