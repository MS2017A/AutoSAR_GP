//*****************************************************************************
//
// COM_task.c - A simple flashing COM task.
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
#include "driverlib/gpio.h"
#include "driverlib/rom.h"
#include "drivers/rgb.h"
#include "drivers/buttons.h"
#include "utils/uartstdio.h"
#include "ComInitTask.h"
#include "priorities.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "Com.h"

extern uint8 Switch1,Switch2;

//*****************************************************************************
//
// The stack size for the COM toggle task.
//
//*****************************************************************************
#define COMTASKSTACKSIZE        5120         // Stack size in words

//*****************************************************************************
//
// The item size and queue size for the COM message queue.
//
//*****************************************************************************
#define COM_ITEM_SIZE           sizeof(uint8_t)
#define COM_QUEUE_SIZE          5

//*****************************************************************************
//
// Default COM toggle delay value. COM toggling frequency is twice this number.
//
//*****************************************************************************
#define COM_TOGGLE_DELAY        100

//*****************************************************************************
//
// The queue that holds messages sent to the COM task.
//
//*****************************************************************************
xQueueHandle g_pCOMQueue;

extern xSemaphoreHandle g_pUARTSemaphore;

//*****************************************************************************
//
// This task toggles the user selected COM at a user selected frequency. User
// can make the selections by pressing the left and right buttons.
//
//*****************************************************************************
static void
COMTask(void *pvParameters)
{
    uint8 condition=0;
    while(1)
    {
        if(condition==0)
        {
            Com_MainFunctionTx();
            //UARTprintf("\nCOM_Tx is executed\n");
            condition=1;
        }
        else
        {
            condition=0;
        }
        Com_MainFunctionRx();
        //UARTprintf("\nCOM_Rx is executed\n");

        vTaskDelay(COM_TOGGLE_DELAY);
    }
}

//*****************************************************************************
//
// Initializes the COM task.
//
//*****************************************************************************
uint32_t
COMTaskInit(void)
{
    Com_Init(0);
    //
    // Print the current loggling COM and frequency.
    //
    UARTprintf("\nCOM_Stack has been activated\n");

    //
    // Create a queue for sending messages to the COM task.
    //
    g_pCOMQueue = xQueueCreate(COM_QUEUE_SIZE, COM_ITEM_SIZE);

    //
    // Create the COM task.
    //
    if(xTaskCreate(COMTask, (const portCHAR *)"COM", COMTASKSTACKSIZE, NULL,
                   tskIDLE_PRIORITY + PRIORITY_COM_TASK, NULL) != pdTRUE)
    {
        return(1);
    }

    //
    // Success.
    //
    return(0);
}
