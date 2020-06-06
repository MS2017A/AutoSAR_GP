//*****************************************************************************
//
// switch_task.c - A simple switch task to process the buttons.
//
// Copyright (c) 2012-2017 Texas Instruments Incorporated.  All rights reserved.
// Software License Agreement
// 
// Texas Instruments (TI) is supplying this software for use solely and
// exclusively on TI's microcontroller products. The software is owned by
// TI and/or its suppliers, and is protected under applicable copyright
// laws. You may not combine this software with "viral" open-source
// software in order to form a larger program.
// 
// THIS SOFTWARE IS PROVIDED "AS IS" AND WITH ALL FAULTS.
// NO WARRANTIES, WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT
// NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. TI SHALL NOT, UNDER ANY
// CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL, OR CONSEQUENTIAL
// DAMAGES, FOR ANY REASON WHATSOEVER.
// 
// This is part of revision 2.1.4.178 of the EK-TM4C123GXL Firmware Package.
//
//*****************************************************************************

#include <stdbool.h>
#include <stdint.h>
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "inc/hw_gpio.h"
#include "driverlib/sysctl.h"
#include "driverlib/gpio.h"
#include "driverlib/rom.h"
#include "drivers/buttons.h"
#include "utils/uartstdio.h"
#include "switch_task.h"
#include "led_task.h"
#include "priorities.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include "Com.h"

//uint8 Switch1,Switch2;

//*****************************************************************************
//
// The stack size for the display task.
//
//*****************************************************************************
#define SWITCHTASKSTACKSIZE        128         // Stack size in words

extern xQueueHandle g_pLEDQueue;
extern xSemaphoreHandle g_pUARTSemaphore;

//*****************************************************************************
//
// This task reads the buttons' state and passes this information to LEDTask.
//
//*****************************************************************************
static void
SwitchTask(void *pvParameters)
{
    portTickType ui16LastTime;
    uint32_t ui32SwitchDelay = 25;
    uint8_t ui8CurButtonState, ui8PrevButtonState;
    uint8_t Data;
    uint8_t Switch_Status[2];

    ui8CurButtonState = ui8PrevButtonState = 0;

    //
    // Get the current tick count.
    //
    ui16LastTime = xTaskGetTickCount();

    //
    // Loop forever.
    //
    while(1)
    {
        //
        // Poll the debounced state of the buttons.
        //
        ui8CurButtonState = ButtonsPoll(0, 0);

        //
        // Check if previous debounced state is equal to the current state.
        //
        if(ui8CurButtonState != ui8PrevButtonState)
        {
            ui8PrevButtonState = ui8CurButtonState;

            //
            // Check to make sure the change in state is due to button press
            // and not due to button release.
            //
            if((ui8CurButtonState & ALL_BUTTONS) != 0)
            {
                if((ui8CurButtonState & ALL_BUTTONS) == LEFT_BUTTON)
                {
                    Data=1;
//                    Switch1=1;
//                    Switch2=0;
                    Com_SendSignal(heatleft, &Data);
                    UARTprintf("\nCOM_SendSignal Switch1 is executed\n");


                    //
                    // Guard UART from concurrent access.
                    //
                    xSemaphoreTake(g_pUARTSemaphore, portMAX_DELAY);
                    UARTprintf("Left Button is pressed.\n");
                    xSemaphoreGive(g_pUARTSemaphore);
                }
                /*else
                {
                    Data=0;
                    Switch1=0;
                    UARTprintf("Left Button is Released.\n");
                    //Com_SendSignal(heatleft, &Data);
                }*/
                else if((ui8CurButtonState & ALL_BUTTONS) == RIGHT_BUTTON)
                {
                    Data=1;
 //                   Switch2=1;
                    Com_SendSignal(heatright, &Data);
                    UARTprintf("\nCOM_SendSignal Switch2 is executed\n");

                    //
                    // Guard UART from concurrent access.
                    //
                    xSemaphoreTake(g_pUARTSemaphore, portMAX_DELAY);
                    UARTprintf("Right Button is pressed.\n");
                    xSemaphoreGive(g_pUARTSemaphore);
                }
                /*else
                {
                    Data=0;
                    Switch2=0;
                    UARTprintf("Right Button is Released.\n");
                    //Com_SendSignal(heatleft, &Data);
                }*/
            }
            else
            {
//                Switch1=0;
//                Switch2=0;
                Data=0;
                Com_SendSignal(heatleft, &Data);
                UARTprintf("\nCOM_SendSignal Switch1 is executed\n");
                Com_SendSignal(heatright, &Data);
                UARTprintf("\nCOM_SendSignal Switch2 is executed\n");

                UARTprintf("Buttons are Released.\n");
            }
        }

        //
        // Wait for the required amount of time to check back.
        //
        vTaskDelayUntil(&ui16LastTime, ui32SwitchDelay / portTICK_RATE_MS);
    }
}

//*****************************************************************************
//
// Initializes the switch task.
//
//*****************************************************************************
uint32_t
SwitchTaskInit(void)
{
    //
    // Unlock the GPIO LOCK register for Right button to work.
    //
    HWREG(GPIO_PORTF_BASE + GPIO_O_LOCK) = GPIO_LOCK_KEY;
    HWREG(GPIO_PORTF_BASE + GPIO_O_CR) = 0xFF;

    //
    // Initialize the buttons
    //
    ButtonsInit();

    //
    // Create the switch task.
    //
    if(xTaskCreate(SwitchTask, (const portCHAR *)"Switch",
                   SWITCHTASKSTACKSIZE, NULL, tskIDLE_PRIORITY +
                   PRIORITY_SWITCH_TASK, NULL) != pdTRUE)
    {
        return(1);
    }

    //
    // Success.
    //
    return(0);
}
