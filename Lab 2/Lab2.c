//*****************************************************************************
//
// uart_echo.c - Example for reading data from and writing data to the UART in
//               an interrupt driven fashion.
//
// Copyright (c) 2011-2017 Texas Instruments Incorporated.  All rights reserved.
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
// This is part of revision 2.1.4.178 of the DK-TM4C123G Firmware Package.
//
//*****************************************************************************

#include <stdint.h>
#include <stdio.h> //sprintf
#include <stdbool.h>
#include <time.h>
#include <stdlib.h> // malloc
#include <string.h>
//#include "inc/hw_ints.h"
#include "inc/hw_memmap.h" // For BASE calls
#include "driverlib/debug.h"
#include "driverlib/fpu.h" //for FPULazyStackingEnable
#include "driverlib/gpio.h"
//#include "driverlib/interrupt.h"
#include "driverlib/sysctl.h" // For System Control Specs
#include "driverlib/uart.h"
//#include "driverlib/rom.h"
#include "grlib/grlib.h" // For output calls
#include "drivers/cfal96x64x16.h" //For calls of header file name
#include "drivers/buttons.h" // for buttons counter
#define LEDon 20000                 // defines the on period of the LED in ms
#define LEDoff 380000               // defines the off period of the LED in ms

//*****************************************************************************
//
//! \addtogroup example_list
//! <h1>UART Echo (uart_echo)</h1>
//!
//! This example application utilizes the UART to echo text.  The first UART
//! (connected to the USB debug virtual serial port on the evaluation board)
//! will be configured in 115,200 baud, 8-n-1 mode.  All characters received on
//! the UART are transmitted back to the UART.
//
//*****************************************************************************

//*****************************************************************************
//
// The error routine that is called if the driver library encounters an error.
//
//*****************************************************************************
#ifdef DEBUG
void
__error__(char *pcFilename, uint32_t ui32Line)
{
}
#endif

//*****************************************************************************
//
// The UART interrupt handler.
//
//*****************************************************************************
void clear(void);
void putString(char *str);
void printMenu(void);
void blinky(volatile uint32_t ui32Loop);

void UARTIntHandler(void)
{
    uint32_t ui32Status;
    
    //
    // Get the interrrupt status.
    //
    ui32Status = UARTIntStatus(UART0_BASE, true);
    
    //
    // Clear the asserted interrupts.
    //
    UARTIntClear(UART0_BASE, ui32Status);
    
    //
    // Loop while there are characters in the receive FIFO.
    //
    while(UARTCharsAvail(UART0_BASE))
    {
        //
        // Read the next character from the UART and write it back to the UART.
        //
        UARTCharPutNonBlocking(UART0_BASE,
                               UARTCharGetNonBlocking(UART0_BASE));
    }
}

//*****************************************************************************
//
// Holds the current, debounced state of each button.  A 0 in a bit indicates
// that that button is currently pressed, otherwise it is released.
// We assume that we start with all the buttons released (though if one is
// pressed when the application starts, this will be detected).
//
//*****************************************************************************
static uint8_t g_ui8ButtonStates = ALL_BUTTONS;

//*****************************************************************************
//
//! Initializes the GPIO pins used by the board pushbuttons.
//!
//! This function must be called during application initialization to
//! configure the GPIO pins to which the pushbuttons are attached.  It enables
//! the port used by the buttons and configures each button GPIO as an input
//! with a weak pull-up.
//!
//! \return None.
//
//*****************************************************************************
void
ButtonsInit(void)
{
    //
    // Enable the GPIO port to which the pushbuttons are connected.
    //
    SysCtlPeripheralEnable(BUTTONS_GPIO_PERIPH);
    
    //
    // Set each of the button GPIO pins as an input with a pull-up.
    //
    GPIODirModeSet(BUTTONS_GPIO_BASE, ALL_BUTTONS, GPIO_DIR_MODE_IN);
    GPIOPadConfigSet(BUTTONS_GPIO_BASE, ALL_BUTTONS,
                     GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);
    
    //
    // Initialize the debounced button state with the current state read from
    // the GPIO bank.
    //
    g_ui8ButtonStates = GPIOPinRead(BUTTONS_GPIO_BASE, ALL_BUTTONS);
}

//*****************************************************************************
//
// Send a string to the UART.
//
//*****************************************************************************
void
UARTSend(const uint8_t *pui8Buffer, uint32_t ui32Count)
{
    //
    // Loop while there are more characters to send.
    //
    while(ui32Count--)
    {
        //
        // Write the next character to the UART.
        //
        UARTCharPutNonBlocking(UART0_BASE, *pui8Buffer++);
    }
}


//*****************************************************************************
//
// This example demonstrates how to send a string of data to the UART.
//
//*****************************************************************************
int
main(void)
{
    tRectangle sRect;
    tContext sContext;
    int gNumCharRecv = 0;
    volatile uint32_t ui32Loop;
    bool shouldFlood = false;
    bool shouldBlink = true;
    int tickCount = 0;
    int whileLoop = 1;
    int colorSwitch = 0;
    int shouldCycle = 0;
    uint32_t buttonState = 0;
    int buttonCounter = 0;
    int lastPressed = 0;
    //
    // Enable lazy stacking for interrupt handlers.  This allows floating-point
    // instructions to be used within interrupt handlers, but at the expense of
    // extra stack usage.
    //
    FPULazyStackingEnable();
    
    //
    // Set the clocking to run directly from the crystal.
    //
    SysCtlClockSet(SYSCTL_SYSDIV_1 | SYSCTL_USE_OSC | SYSCTL_OSC_MAIN |
                   SYSCTL_XTAL_16MHZ);
    
    //
    // Enable the GPIO port that is used for the on-board LED.
    //
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOG);
    
    //
    // Check if the peripheral access is enabled.
    //
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOG))
    {
    }
    
    //
    // Enable the GPIO pin for the LED (PG2).  Set the direction as output, and
    // enable the GPIO pin for digital function.
    //
    GPIOPinTypeGPIOOutput(GPIO_PORTG_BASE, GPIO_PIN_2);
    
    
    //
    // Initialize the display driver.
    //
    CFAL96x64x16Init();
    
    //
    // Initialize the graphics context.
    //
    GrContextInit(&sContext, &g_sCFAL96x64x16);
    
    //
    // Fill the top part of the screen with blue to create the banner.
    //
    sRect.i16XMin = 0;
    sRect.i16YMin = 0;
    sRect.i16XMax = GrContextDpyWidthGet(&sContext) - 1;
    sRect.i16YMax = 9;
    GrContextForegroundSet(&sContext, ClrDarkBlue);
    GrRectFill(&sContext, &sRect);
    
    //
    // Change foreground for white text.
    //
    GrContextForegroundSet(&sContext, ClrWhite);
    
    //
    // Put the application name in the middle of the banner.
    //
    GrContextFontSet(&sContext, g_psFontFixed6x8);
    GrStringDrawCentered(&sContext, "Gray & Pietz", -1,
                         GrContextDpyWidthGet(&sContext) / 2, 4, 0);
    
    //
    // Initialize the display and write some instructions.
    //
    GrStringDrawCentered(&sContext, "Choose an", -1,
                         GrContextDpyWidthGet(&sContext) / 2, 20, false);
    GrStringDrawCentered(&sContext, "option", -1,
                         GrContextDpyWidthGet(&sContext) / 2, 30, false);
    GrStringDrawCentered(&sContext, "from the", -1,
                         GrContextDpyWidthGet(&sContext) / 2, 40, false);
    GrStringDrawCentered(&sContext, "menu.", -1,
                         GrContextDpyWidthGet(&sContext) / 2, 50, false);
    
    //
    // Enable the peripherals used by this example.
    //
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
    
    //
    // Enable processor interrupts.
    //
    //IntMasterEnable();
    
    //
    // Set GPIO A0 and A1 as UART pins.
    //
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);
    
    //
    // Configure the UART for 115,200, 8-N-1 operation.
    //
    UARTConfigSetExpClk(UART0_BASE, SysCtlClockGet(), 115200,
                        (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE |
                         UART_CONFIG_PAR_NONE));
    
    //
    // Enable the UART interrupt.
    //
    // ROM_IntEnable(INT_UART0);
    //ROM_UARTIntEnable(UART0_BASE, UART_INT_RX | UART_INT_RT);
    
    //
    // Prompt for text to be entered.
    //
    // UARTSend((uint8_t *)"Enter text: ", 12);
    
    //
    // Loop forever echoing data through the UART.
    //
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOG);
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOG))
    {
    }
    
    GPIOPinTypeGPIOOutput(GPIO_PORTG_BASE, GPIO_PIN_2);
    ButtonsInit();
    int Looper = 0;
    clear();
    printMenu();
    while(whileLoop != 0) {
        Looper++;
        whileLoop++;
        tickCount++;
        if(tickCount == 3) {
            tickCount = 0;
            if(shouldFlood == 1) {
                UARTCharPut(UART0_BASE, '@');
            }
        }
        if(shouldBlink) {
            blinky(ui32Loop);
        }
        buttonState = (~GPIOPinRead(BUTTONS_GPIO_BASE,ALL_BUTTONS) && ALL_BUTTONS);
        if(GPIOPinRead(BUTTONS_GPIO_BASE, ALL_BUTTONS)) {
            buttonCounter++;
            switch (buttonState) {
                case UP_BUTTON:
                    lastPressed = 1;
                    break;
                case DOWN_BUTTON:
                    lastPressed = 2;
                    break;
                case LEFT_BUTTON:
                    lastPressed = 3;
                    break;
                case RIGHT_BUTTON:
                    lastPressed = 4;
                    break;
                case SELECT_BUTTON:
                    lastPressed = 5;
                    break;
            }
        }
        if(shouldCycle == 1) {
            if(colorSwitch == 2) {
                colorSwitch = -1;
            }
            colorSwitch++;
            sRect.i16XMin = 0;
            sRect.i16YMin = 0;
            sRect.i16XMax = GrContextDpyWidthGet(&sContext) - 1;
            sRect.i16YMax = GrContextDpyHeightGet(&sContext) - 1;
            switch (colorSwitch) {
                case 0:
                    GrContextForegroundSet(&sContext, ClrDarkBlue);
                    GrContextBackgroundSet(&sContext, ClrDarkBlue);
                    break;
                case 1:
                    GrContextForegroundSet(&sContext, ClrRed);
                    GrContextBackgroundSet(&sContext, ClrRed);
                    break;
                case 2:
                    GrContextForegroundSet(&sContext, ClrGreen);
                    GrContextBackgroundSet(&sContext, ClrGreen);
                    break;
                default:
                    GrContextForegroundSet(&sContext, ClrDarkBlue);
                    GrContextBackgroundSet(&sContext, ClrDarkBlue);
                    break;
            }
            GrRectFill(&sContext, &sRect);
            GrContextForegroundSet(&sContext, ClrWhite);
            GrContextFontSet(&sContext, g_psFontFixed6x8);
            GrStringDrawCentered(&sContext, "Gray & Pietz", -1,
                                 GrContextDpyWidthGet(&sContext) / 2, 4, 0);
        }
        
        
        
        while(UARTCharsAvail(UART0_BASE)) {
            int32_t local_char;
            sRect.i16XMin = 0;
            sRect.i16YMin = 10;
            sRect.i16XMax = GrContextDpyWidthGet(&sContext) - 1;
            sRect.i16YMax = GrContextDpyHeightGet(&sContext) - 1;
            GrContextForegroundSet(&sContext, ClrBlack);
            GrRectFill(&sContext, &sRect);
            GrContextForegroundSet(&sContext, ClrWhite);
            char str[50];
            sprintf(str,"Loop: %d",Looper);
            GrStringDrawCentered(&sContext, str, -1,
                                 GrContextDpyWidthGet(&sContext) / 2, 20, false);
            sprintf(str,"Last Pressed: %d",lastPressed);
            GrStringDrawCentered(&sContext, str, -1,
                                 GrContextDpyWidthGet(&sContext) / 2, 30, false);
            sprintf(str,"Press Counter: %d",buttonCounter);
            GrStringDrawCentered(&sContext, str, -1,
                                 GrContextDpyWidthGet(&sContext) / 2, 40, false);
            local_char = UARTCharGetNonBlocking(UART0_BASE);
            
            
            if (local_char != -1) {
                gNumCharRecv++; // Counts characters recieved
                UARTCharPut(UART0_BASE, local_char);
                switch(local_char) {
                    case 76: //LED L
                        if(shouldBlink == 0)
                            shouldBlink = 1;
                        else
                            shouldBlink = 0;
                        break;
                    case 70: //Flood F
                        if(shouldFlood == 0) {
                            shouldFlood = 1;
                        }
                        else {
                            shouldFlood = 0;
                        }
                        break;
                    case 69: //Clear E
                        clear();
                        break;
                    case 81: // Quit Q
                        putString("\n\rBYE!");
                        sRect.i16XMin = 0;
                        sRect.i16YMin = 0;
                        sRect.i16XMax = GrContextDpyWidthGet(&sContext) - 1;
                        sRect.i16YMax = GrContextDpyHeightGet(&sContext) - 1;
                        GrContextForegroundSet(&sContext, ClrBlack);
                        GrContextBackgroundSet(&sContext, ClrBlack);
                        GrRectFill(&sContext, &sRect);
                        GrContextForegroundSet(&sContext, ClrRed);
                        GrContextFontSet(&sContext, g_psFontFixed6x8);
                        GrStringDrawCentered(&sContext, "Goodbye", -1,
                                             GrContextDpyWidthGet(&sContext) / 2, 30, false);
                        whileLoop = 0;
                        break;
                    case 77: // Menu M
                        printMenu();
                        break;
                    case 67: // Color C
                        if(colorSwitch == 2) {
                            colorSwitch = -1;
                            
                        }
                        colorSwitch++;
                        sRect.i16XMin = 0;
                        sRect.i16YMin = 0;
                        sRect.i16XMax = GrContextDpyWidthGet(&sContext) - 1;
                        sRect.i16YMax = 9;
                        switch (colorSwitch) {
                            case 0:
                                GrContextForegroundSet(&sContext, ClrDarkBlue);
                                GrContextBackgroundSet(&sContext, ClrDarkBlue);
                                break;
                            case 1:
                                GrContextForegroundSet(&sContext, ClrRed);
                                GrContextBackgroundSet(&sContext, ClrRed);
                                break;
                            case 2:
                                GrContextForegroundSet(&sContext, ClrGreen);
                                GrContextBackgroundSet(&sContext, ClrGreen);
                                break;
                            default:
                                GrContextForegroundSet(&sContext, ClrDarkBlue);
                                GrContextBackgroundSet(&sContext, ClrDarkBlue);
                                break;
                        }
                        GrRectFill(&sContext, &sRect);
                        GrContextForegroundSet(&sContext, ClrWhite);
                        GrContextFontSet(&sContext, g_psFontFixed6x8);
                        GrStringDrawCentered(&sContext, "Gray & Pietz", -1,
                                             GrContextDpyWidthGet(&sContext) / 2, 4, 0);
                        break;
                        
                    case 80: // Color Cycle P
                        if(shouldCycle == 0) {
                            shouldCycle = 1;
                        }
                        else
                        {
                            shouldCycle = 0;
                        }
                        break;
                        
                } //end switch
            }
        }
    } //end infinite while
} //end main


void clear() {
    UARTCharPut(UART0_BASE, 12);
}

void putString(char *str) {
    for(int i = 0; i < strlen(str); i++) {
        UARTCharPut(UART0_BASE, str[i]);
    }
}

void printMenu() {
    char*menu = "\rMenu Selection: \n\rP - Party Mode\n\rC - Change Background Color\n\rE - Erase Terminal Window\n\rL - Flash LED\n\rF - Flood Character\n\rM - Print the Menu\n\rQ - Quit this program\n\r";
    putString(menu, UART0_BASE);
}

void blinky(volatile uint32_t ui32Loop) {
    //
    // Turn on the LED.
    //
    GPIOPinWrite(GPIO_PORTG_BASE, GPIO_PIN_2, GPIO_PIN_2);
    
    //
    // Delay for amount specified in the LEDon #define at the top.
    //
    for(ui32Loop = 0; ui32Loop < LEDon; ui32Loop++)
    {
    }
    
    //
    // Turn off the LED.
    //
    GPIOPinWrite(GPIO_PORTG_BASE, GPIO_PIN_2, 0);
    //
    
    // Delay for amount specified in the LEDoff #define at the top.
    //
    for(ui32Loop = 0; ui32Loop < LEDoff; ui32Loop++)
    {
    }
    
}
