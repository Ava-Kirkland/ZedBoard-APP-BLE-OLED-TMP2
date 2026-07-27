/**
 * main.c
 * ZedBoard + Custom BLE APP + Pmod BLE + ADT7420 Temperature Sensor + OLED Display
 * Reads temperature via I2C every 200ms and after 5 good readings the average is sent to be displayed on the peripherials ~1Hz
 * Celsius and Fahrenheit on the on-board OLED and APP (if connected)
 * OLED turns on at the start of application, turns off when a phone is disconnected, and turns back on when a phone reconnects
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
#define NUM_SAMPLES 5
#define SAMPLE_INTERVAL (COUNTS_PER_SECOND/ NUM_SAMPLES)






void BLE_ParseCommand(char *buf, bool *connected, bool *streaming, bool * oled_on, OLED_Control_t *my_oled);


int main() {
    OLED_Control_t my_oled;
    XTime last_sample;
    XTime now;
    XIicPs i2c;
    //added
    u8 newline[2] = {'\r', '\n'};
    int status;
    u8 rx_c;
    u32 received;
    char buf[64] = {0};
    bool streaming = false;
    bool connected = false;
    bool cmd_mode = false;
    bool oled_on = true;
    u32 buf_index =0;
    char temp_line[TEMP_LINE_LEN + 1];
    int dollar_count = 0;
    int dash_count = 0;
    
    float sample = 0.00f;
    float  acc_total     = 0.0f;
    int    acc_count     = 0;
    bool   temp_ready    = false;
    float  avg_temp      = 0.0f;


    // Init OLED first — confirms display is alive before I2C init runs
    if (oled_on){
        OLED_Init(&my_oled, XPAR_OLEDADDITION_0_BASEADDR);

        OLED_Clear(&my_oled);
    }


//added


    status = UART_Init();
    if (status != XST_SUCCESS){
        xil_printf("ERROR: UART init");
        return XST_FAILURE;
    }
    // needs to be after init_uart so the sleeper timer is initialized before gets to this line


    if(oled_on){
        OLED_PrintLine(&my_oled, "=== TEMP SENSOR=");
        OLED_PrintLine(&my_oled, "                ");
        OLED_PrintLine(&my_oled, "Initializing... ");
        OLED_PrintLine(&my_oled, "                ");
        sleep(2); // hold splash long enough to be visible
    }


    xil_printf("=== ADT7420 + OLED Integration ===\r\n");


    if (ADT7420_Init(&i2c, I2C_BASEADDR) != XST_SUCCESS && oled_on) {
        xil_printf("ADT7420: init failed — halting\r\n");
        OLED_PrintLine(&my_oled, "=== TEMP SENSOR=");
        OLED_PrintLine(&my_oled, "                ");
        OLED_PrintLine(&my_oled, "I2C INIT FAILED ");
        OLED_PrintLine(&my_oled, "CHECK HARDWARE  ");
        return -1;
    }


    XTime_GetTime(&last_sample); // void function, writes into your variable via pointer

    
    while (1) {


        if(!cmd_mode){
            XTime_GetTime(&now);
            if((now - last_sample) >= SAMPLE_INTERVAL){

            
                sample = ADT7420_ReadTemperature(&i2c);
                if(sample > ADT7420_SENTINEL_THRESHOLD){
                    acc_total += sample;
                    acc_count++;
                }

                if(acc_count >= NUM_SAMPLES){
                    avg_temp = acc_total / (float)acc_count;
                    temp_ready = true;
                    acc_count = 0;
                    acc_total = 0.00f;
                }
                last_sample = now;
            }
                
            if(temp_ready)
                {
                    temp_ready = false;
                                    //update OLED

                if (avg_temp <= ADT7420_SENTINEL_THRESHOLD) {
                    xil_printf("ADT7420: read error (code: %d) - check pull-ups, address, wiring\r\n", (int)avg_temp);
                    if (streaming) {
                        XUartPs_Send(&Uart0, (u8*)"ERROR:SENSOR_FAIL\r\n", 19);
                    }
                } else{
                    TempParts temp_c = ADT7420_DecomposeTemp(avg_temp);
                    TempParts temp_f = ADT7420_DecomposeTemp(celsius_to_fahrenheit(avg_temp));
                    ADT7420_PrintTempParts(&temp_c, &temp_f);                   // UART output
                    if(oled_on) OLED_DisplayTempParts(&my_oled, &temp_c, &temp_f);    
                      // OLED output
                    if (streaming) {


                        snprintf(temp_line, sizeof(temp_line), "TEMP:%s%d.%02dC,%s%d.%02dF\r\n",
                            (temp_c.sign < 0 ? "-" : ""), temp_c.whole, temp_c.frac,
                            (temp_f.sign < 0 ? "-" : ""), temp_f.whole, temp_f.frac);
                        XUartPs_Send(&Uart0, (u8*)temp_line, strlen(temp_line));
                    }
                }
            }
           
        }


        // Terminal → BLE
        if (XUartPs_IsReceiveData(Uart1.Config.BaseAddress)) {


            //Receives 1 character
            received = XUartPs_Recv(&Uart1, &rx_c, 1);
        if (received == 1){


            if ( rx_c == '\r'){
   
                XUartPs_Send(&Uart1, newline, 2);
            }else{
                //Echo Terminal                
                XUartPs_Send(&Uart1, &rx_c, 1);                      
            }
                //U0 - BLE
                XUartPs_Send(&Uart0, &rx_c, 1);
           
            //$$$ command doesn't have a \n so can't put it in te parse_command function
            if(rx_c == '$'){
                dollar_count++;
                if(dollar_count >=3){
                    cmd_mode = true;
                    dollar_count = 0;
                }
               
            }else {
                dollar_count = 0;
            }
            if (cmd_mode && rx_c == '-'){
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
       
            received = XUartPs_Recv(&Uart0, &rx_c, 1);
           
        if (received == 1){
            XUartPs_Send(&Uart1, &rx_c, 1); // always send to Terminal


       
           
         if (!cmd_mode){
            if (buf_index >= 63){
                buf_index =0;
            }
           
            buf[buf_index] = rx_c;
            buf_index++;
            if( rx_c == '\n' && buf_index > 0 && buf[buf_index -2] == '\r'){
                buf[buf_index -2] = '\0';
                BLE_ParseCommand(buf, &connected, &streaming, &oled_on, &my_oled);
                buf_index =0;
            }
            else if (buf_index > 1 && rx_c == '%' && buf[0] == '%'){
                buf[buf_index] = '\0';
                BLE_ParseCommand(buf, &connected, &streaming, &oled_on, &my_oled);
                buf_index = 0;
            }
        }
        }
    }
    }
    return 0;
}




void BLE_ParseCommand(char *buf, bool *connected, bool *streaming, bool *oled_on, OLED_Control_t *my_oled){
    //Line below is for debugfing what is in the buffer
    //xil_printf("DEBUG parse: [%s] first=%d\r\n", buf, (int)buf[0]);
    switch(buf[0]){
        case '%':
            if (strstr(buf, STREAM) != NULL){
                *connected = true;
                if(oled_on != NULL && my_oled != NULL && (!(*oled_on))){
                    OLED_RepowerOn(my_oled);
                    *oled_on = true;
                }
            } else if (strstr(buf, DISCONNECT) != NULL){
                *connected = false;
                *streaming = false;
                if(my_oled != NULL ) OLED_Off(my_oled);
                if(oled_on != NULL) *oled_on = false;
                
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

// NOTE: Averaging (5 samples, 200ms apart) was implemented but removed because
// blocking I2C reads inside getAverageTemp() prevented UART polling — $$$ cmd
// mode entry and BLE START_TEMP processing became unreliable.
// Nonblocking polling of reading 5 temperatures at intervals of 200ms, after getting 5 valid readings, send the average to the peripherials
//Future Improvments: force a name standard, struct for app state to decrase the parse command call, enable functionality for 2 Pmod BLE and 2 phones connecting and disconnecting