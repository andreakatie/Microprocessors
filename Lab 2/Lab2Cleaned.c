//*******************************************************************************
//
// CS322.50 Labratory 2 Software File
// Developed by: Andrea Gray and Daniel Pietz (c)
// Version: 1.30 28-JAN-2019
//
//*******************************************************************************

//*******************************************************************************
//
// uart_echo.c - Example source for reading data from and writing data to the 
//				 UART in an interrupt driven fashion for Labratory Exercise 2. 
//
// Copyright (c) 2011-2017 Texas Instruments Incorporated. 
//
// This is part of revision 2.1.4.178 of the DK-TM4C123G Firmware Package.
//
//*******************************************************************************

//*******************************************************************************
//
// 							!!! DISCLAIMER !!!
//
// Functions in the code below have been wrapped without syntax consideration
// in referfence to compiling to fit lab report width specifications.
// The code file (.c) used in the execution of the project does not include 
// these text wraps for proper compilation. 
//
//*******************************************************************************

#include <stdint.h>					// Standard library header for integers 
									// with varying widths
#include <stdio.h> 					// Standard library header for input
									// and outputs -- sprintf in this
									// program specifically 
#include <stdbool.h>				// Standard library header for boolean
									// data types
#include <time.h>					// Header file with four main variable
									// types for manipulating date and 
									// time information
#include <stdlib.h> 				// Standard C library header -- malloc
									// in this program specifically 
#include <string.h>					// Header file for the use and 
									// manipulation of strings
#include "inc/hw_memmap.h" 			// Header file for BASE call use
#include "driverlib/debug.h"		// TM4C123G debugging header file
#include "driverlib/fpu.h" 			// Header for FPULazyStackingEnable
									// function
#include "driverlib/gpio.h"			// Header file for all GPIO function calls
#include "driverlib/sysctl.h" 		// Header file for System Control Specs
#include "driverlib/uart.h"			// Header file for UART function calls
#include "grlib/grlib.h" 			// Header file for output calls
#include "drivers/cfal96x64x16.h" 	// Header file for OLED display dimension
									// specifications
#include "drivers/buttons.h" 		// Header file for push-buttons counter
#define LEDon 20000                 // defines the on period of the LED in ms
#define LEDoff 380000               // defines the off period of the LED in ms

//*******************************************************************************
//
// This example application utilizes the UART to echo text. All characters
// recoeved on the UART are transmitted back to the UART.
//
//*******************************************************************************

													
//*******************************************************************************
// Function Declarations
//*******************************************************************************
void printScreen(char *str);
void clear();
void printMenu();
void blinky(volatile uint32_t ui32Loop);

//*******************************************************************************
//
// The error routine that is called if the driver library encounters an error.
//
//*******************************************************************************
#ifdef DEBUG
void __error__(char *pcFilename, uint32_t ui32Line) {
}
#endif

//*******************************************************************************
//
// Holds the current, debounced state of each button.  0 = pressed.
// We assume that we start with all the buttons released (though if one is
// pressed when the application starts, this will be detected).
//
//*******************************************************************************
static uint8_t g_ui8ButtonStates = ALL_BUTTONS;

//*******************************************************************************
//
// Initializes the GPIO pins used by the board pushbuttons with a weak 
// pull-up.
//
//*******************************************************************************
void ButtonsInit(void) {
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

//*******************************************************************************
//
// Send a string to the UART.
//
//*******************************************************************************
void UARTSend(const uint8_t *pui8Buffer, uint32_t ui32Count) {
    // Loop while there are more characters to send.
    while(ui32Count--) {
        // Write the next character to the UART.
        UARTCharPutNonBlocking(UART0_BASE, *pui8Buffer++);
    }
}


//*******************************************************************************
//
// The main function to intialize the UART, LED, OLED, and run through the 
// program
//
//*******************************************************************************
int main(void) {
    tRectangle sRect;									// OLED rectangle 
														// variable
    tContext sContext;									// OLED graphics buffer
    int gNumCharRecv = 0;								// Count for characters
														// recieved by the UART 
    volatile uint32_t ui32Loop;							// Blinky LED volatile	
														// loop.
    bool shouldFlood = false;							// "Flood" character 
														// toggle
    bool shouldBlink = true;							// LED blinky toggle
	
	//***************************************************************************
	// Counter variables
	//***************************************************************************
    int tickCount = 0;									// Clock ticks for flood
														// character.
    int whileLoop = 1;								
    int colorSwitch = 0;								// OLED Color toggle
    int shouldCycle = 0;								// Boolean for Party Mode
    uint32_t buttonState = 0;							// Boolean to check if a
														// button was pressed.
    int buttonCounter = 0;								// Counting how many 
														// times a button was  
														// pressed.
    int lastPressed = 0;								// Saves the number of 
														// the last button 	
														// pressed for OLED 
														// output. 
	
	//***************************************************************************
    //
    // Enable lazy stacking for interrupt handlers.  This allows floating-point
    // instructions to be used within interrupt handlers, but at the expense of
    // extra stack usage.
    //
	//***************************************************************************
    FPULazyStackingEnable();
    
	//***************************************************************************
    //
    // Set the clocking to run directly from the crystal.
    //
	//***************************************************************************
    SysCtlClockSet(SYSCTL_SYSDIV_1 | SYSCTL_USE_OSC | SYSCTL_OSC_MAIN |
                   SYSCTL_XTAL_16MHZ);
    
    //***************************************************************************
	//
    // Enable the GPIO port that is used for the on-board LED.
    //
	//***************************************************************************
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOG);
    
    //***************************************************************************
	//
    // Check if the LED peripheral access is enabled.
    //
	//***************************************************************************
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOG)) {
    }
    
    //***************************************************************************
	//
    // Enable the GPIO pin for the LED.  Set the direction as output, and
    // enable the GPIO pin for digital function.
    //
	//***************************************************************************
    GPIOPinTypeGPIOOutput(GPIO_PORTG_BASE, GPIO_PIN_2);
    
    
    //***************************************************************************
	//
    // Initialize the OLED display driver.
    //
	//***************************************************************************
    CFAL96x64x16Init();
    
    //***************************************************************************
	//
    // Initialize the OLED graphics context.
    //
	//***************************************************************************
    GrContextInit(&sContext, &g_sCFAL96x64x16);
    
    //***************************************************************************
	//
    // Fill the top part of the screen with blue to create the banner.
    //
	//***************************************************************************
    sRect.i16XMin = 0;									// left edge
    sRect.i16YMin = 0;									// top edge
    sRect.i16XMax = GrContextDpyWidthGet(&sContext) - 1;// right edge
    sRect.i16YMax = 9;									// bottom edge
    GrContextForegroundSet(&sContext, ClrDarkBlue);		// dark blue banner
    GrRectFill(&sContext, &sRect);						// fill the rectangle
														// area 
    
    //***************************************************************************
	//
    // Change foreground for white text.
    //
	//***************************************************************************
    GrContextForegroundSet(&sContext, ClrWhite);
    
    //***************************************************************************
	//
    // Put the application name in the middle of the banner.
    //
	//***************************************************************************
    GrContextFontSet(&sContext, g_psFontFixed6x8);
    GrStringDrawCentered(&sContext, "Gray & Pietz", -1,
                         GrContextDpyWidthGet(&sContext) / 2, 4, 0);
    
    //***************************************************************************
	//
    // Initialize the display and write instructions.
    //
	//***************************************************************************
    GrStringDrawCentered(&sContext, "Choose an", -1,
                         GrContextDpyWidthGet(&sContext) / 2, 20, false);
    GrStringDrawCentered(&sContext, "option", -1,
                         GrContextDpyWidthGet(&sContext) / 2, 30, false);
    GrStringDrawCentered(&sContext, "from the", -1,
                         GrContextDpyWidthGet(&sContext) / 2, 40, false);
    GrStringDrawCentered(&sContext, "menu.", -1,
                         GrContextDpyWidthGet(&sContext) / 2, 50, false);
    
    //***************************************************************************
	//
    // Enable the peripherals used by this example.
    //
	//***************************************************************************
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);		// GPIO Pin Set
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);		// UART0 Set
		
    
	//***************************************************************************
    //
    // Set GPIO 0 and 1 as UART pins.
    //
	//***************************************************************************
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);
    
    //***************************************************************************
	//
    // Configure the UART for custom 115,200 baud rate.
    //
	//***************************************************************************
    UARTConfigSetExpClk(UART0_BASE, SysCtlClockGet(), 115200,
                        (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE |
                         UART_CONFIG_PAR_NONE));

    
    //***************************************************************************
	//
    // Check to see if the GPIO peripheral is ready.
    //
	//***************************************************************************
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOG);
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOG)) {
    }
    
	//***************************************************************************
	//
    // Set the LED Pin output to pin 2. 
    //
	//***************************************************************************
    GPIOPinTypeGPIOOutput(GPIO_PORTG_BASE, GPIO_PIN_2);	// GPIO output is pin 2.
    ButtonsInit();										// Enable buttons for 
														// each loop iteration.
    int Looper = 0;										// Set loop variable and
														// initialize to 0.
    clear();
    printMenu();										// Print the menu out.
	
	//***************************************************************************
	//
	// Indefinite while loop 
	//
	//***************************************************************************
    while(whileLoop != 0) {									
	
		// Variable incrementations.
        Looper++;
        whileLoop++;
        tickCount++;
		
		//***********************************************************************
		//
		// Flood character output description and timing.
		//
		//***********************************************************************
        if(tickCount == 3) {
            tickCount = 0;
            if(shouldFlood == 1) {
                UARTCharPut(UART0_BASE, '@');			// Toggle catch
            }
        }
		
		//***********************************************************************
		//
		// Blinky function toggle catch
		//
		//***********************************************************************
        if(shouldBlink) {
            blinky(ui32Loop);
        }
	
		//***********************************************************************
		//
		// Checking for a button press and displaying output as designated and 
		// incrementing counts accordingly.
		//
		//***********************************************************************
        buttonState = (~GPIOPinRead(BUTTONS_GPIO_BASE,ALL_BUTTONS)
						&& ALL_BUTTONS);
        if(GPIOPinRead(BUTTONS_GPIO_BASE, ALL_BUTTONS)) {
            buttonCounter++;
            switch (buttonState) {
                case UP_BUTTON:
                    lastPressed = 1;					// UP = 1
                    break;
                case DOWN_BUTTON:
                    lastPressed = 2;					// DOWN = 2
                    break;
                case LEFT_BUTTON:
                    lastPressed = 3;					// LEFT = 3
                    break;
                case RIGHT_BUTTON:
                    lastPressed = 4;					// RIGHT = 4
                    break;
                case SELECT_BUTTON:	
                    lastPressed = 5;					// SELECT = 5
                    break;
            }
        }
		
		//***********************************************************************
		//
		// Changing the OLED output depending on Party Mode toggle and Banner 
		// Color menu selection.
		//
		//***********************************************************************
        if(shouldCycle == 1) {
            if(colorSwitch == 2) {
                colorSwitch = -1;						// End case catch
            }	
			
			// Setting OLED display as full avaliable rectangle output.
            colorSwitch++;
            sRect.i16XMin = 0;
            sRect.i16YMin = 0;
            sRect.i16XMax = GrContextDpyWidthGet(&sContext) - 1;
            sRect.i16YMax = GrContextDpyHeightGet(&sContext) - 1;
			
			// Switch case to determine the next OLED color output.
            switch (colorSwitch) {
                case 0:									// 0 = Dark Blue
                    GrContextForegroundSet(&sContext, ClrDarkBlue);
                    GrContextBackgroundSet(&sContext, ClrDarkBlue);
                    break;
                case 1:									// 1 = Red
                    GrContextForegroundSet(&sContext, ClrRed);
                    GrContextBackgroundSet(&sContext, ClrRed);
                    break;
                case 2:									// 2 = Green
                    GrContextForegroundSet(&sContext, ClrGreen);
                    GrContextBackgroundSet(&sContext, ClrGreen);
                    break;
                default:								// Default = Dark Blue
                    GrContextForegroundSet(&sContext, ClrDarkBlue);
                    GrContextBackgroundSet(&sContext, ClrDarkBlue);
                    break;
            }
			
			// Re-draw the OLED rectangular output with above determined color
			// and correct banner message.
            GrRectFill(&sContext, &sRect);
            GrContextForegroundSet(&sContext, ClrWhite);
            GrContextFontSet(&sContext, g_psFontFixed6x8);
            GrStringDrawCentered(&sContext, "Gray & Pietz", -1,
                                 GrContextDpyWidthGet(&sContext) / 2, 4, 0);
        } 													// end shouldCycle()
        
        //***********************************************************************
		//
		// While loop when a character is inputted in the PuTTY window.
		//
		//***********************************************************************
        while(UARTCharsAvail(UART0_BASE)) {
			char str[50];								// Empty string 
														// declaration for 
														// line 469
            int32_t local_char;							// Setting the input 
														// character to a
														// local variable.
			
			//*******************************************************************
			//
			// Re-draw the OLED with updated statistics
			//
			//*******************************************************************
            sRect.i16XMin = 0;
            sRect.i16YMin = 10;
            sRect.i16XMax = GrContextDpyWidthGet(&sContext) - 1;
            sRect.i16YMax = GrContextDpyHeightGet(&sContext) - 1;
            GrContextForegroundSet(&sContext, ClrBlack);
            GrRectFill(&sContext, &sRect);
            GrContextForegroundSet(&sContext, ClrWhite);
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
            
            //*******************************************************************
			//
			// If the input character is not invalid, begin the character
			// matching statment below through the switch statment and act 
			// accordingly to the user input.
			//
			//*******************************************************************
            if (local_char != -1) {
                gNumCharRecv++; 						// Characters recieved
														// counter incrementation
                UARTCharPut(UART0_BASE, local_char);	// Sending a single 
														// character through the		
														// UART TX (transmitting)
														// channel
														
				// Begin character input matching to menu option.
                switch(local_char) {
                    case 67: 							// Switch banner color 
														// - C
                        if(colorSwitch == 2) {
                            colorSwitch = -1;
                        }
                        colorSwitch++;					// Color cycle counter	
														// incrementation
										
						// sRect below is specifying banner area.
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
						
						// Filling in the rest of the OLED screen after banner is 
						// updated.
                        GrRectFill(&sContext, &sRect);
                        GrContextForegroundSet(&sContext, ClrWhite);
                        GrContextFontSet(&sContext, g_psFontFixed6x8);
                        GrStringDrawCentered(&sContext, "Gray & Pietz", -1,
                                             GrContextDpyWidthGet(&sContext) / 2,
											 4, 0);
                        break;
                        
					case 69: 							//Clear PuTTY window - E
                        clear();
                        break;

					case 70: 							// Flood toggle - F
                        if(shouldFlood == 0) {
                            shouldFlood = 1;
                        }
                        else {
                            shouldFlood = 0;
                        }
                        break;
						
					case 76: 							//LED toggle - L
                        if(shouldBlink == 0)
                            shouldBlink = 1;
                        else
                            shouldBlink = 0;
                        break;
						
					case 77: 							// Re-print menu - M
                        printMenu();
                        break;
						
                    case 80: 							// Party Mode Toggle - P
                        if(shouldCycle == 0) {
                            shouldCycle = 1;
                        }
                        else
                        {
                            shouldCycle = 0;
                        }
                        break;
						
					case 81: 							// Quit program - Q
                        putString("\n\rBYE!");			// Goodbye message to
														// PuTTY window.
															
						// Re-draw OLED with goodbye statment in red font.
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
                                             GrContextDpyWidthGet(&sContext) / 2,
											 30, false);
                        whileLoop = 0;
                        break;
						
                } 										// End menu switch block
            }											// End valid character 
														// check 
        }												// End character 
														// detection
														// loop.
    } 													// End indefinite while()
} 														// End of main()

//*******************************************************************************
//
// PuTTY window clearing function.
//
//*******************************************************************************
void clear() {
    UARTCharPut(UART0_BASE, 12);
}

//*******************************************************************************
//
// Using the character output function as a base for a parent function
// used to output an entire string to the OLED one character at a time.
//
//*******************************************************************************
void putString(char *str) {
    for(int i = 0; i < strlen(str); i++) {
        UARTCharPut(UART0_BASE, str[i]);
    }
}

//*******************************************************************************
//
// Print menu function that takes the complete menu as a string and 
// utilizes the print string function created above to output the entire menu
// in one transmission block to the PuTTY window.
//
//*******************************************************************************
void printMenu() {
	// string below is wrapped for printing and elongated for compiling.
    char*menu = "\rMenu Selection: \n\rP - Party Mode\n\rC - Change Background 
					Color\n\rE - Erase Terminal Window\n\rL - Flash LED\n\rF 
					- Flood Character\n\rM - Print the Menu\n\rQ - Quit this 
					program\n\r";
    putString(menu, UART0_BASE);
}

//*******************************************************************************
//
// Blinky LED "heartbeat" function.
//
//*******************************************************************************
void blinky(volatile uint32_t ui32Loop) {
    // Turn on the LED.
    GPIOPinWrite(GPIO_PORTG_BASE, GPIO_PIN_2, GPIO_PIN_2);

    // Delay for amount specified in the LEDon #define at the top.
    for(ui32Loop = 0; ui32Loop < LEDon; ui32Loop++) {
    }
    
    // Turn off the LED.
    GPIOPinWrite(GPIO_PORTG_BASE, GPIO_PIN_2, 0);
    
    // Delay for amount specified in the LEDoff #define at the top.
    for(ui32Loop = 0; ui32Loop < LEDoff; ui32Loop++) {
    }
}

//*******************************************************************************
// End of program!
//*******************************************************************************