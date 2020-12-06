//*****************************************************************************
//
// CS322.50 Labratory 8 Software File
// Developed by: Andrea Gray (c)
// Version: 1.11 09-APR-2019
//                                                   
//*****************************************************************************

//***************************************************************************** 
//
// Copyright (c) 2011-2017 Texas Instruments Incorporated. 
//
// This is part of revision 2.1.4.178 of the DK-TM4C123G Firmware Package.
//
//*****************************************************************************
#include <stdio.h> // Allows for use of inputs and outputs
#include <string.h> // Allows for use of string values
#include <stdint.h> // Allows for use of integer values
#include <stdbool.h> // Allows for use of boolean values

#include "inc/hw_ints.h" // Inclusion of interrupt header file
#include "inc/hw_memmap.h" // Inclusion of device memory map header file
#include "inc/hw_types.h" // Inclusion of common hardware type header file

#include "driverlib/debug.h" // Allows for ease of debugging practices
#include "driverlib/fpu.h" // Allows for floating point usage
#include "driverlib/gpio.h" // Allows for ease of use for GPIO
#include "driverlib/interrupt.h" // Allows for ease of use of interrupts
#include "driverlib/sysctl.h" // System Control header file
#include "driverlib/timer.h" // Header file inclusion for timing usage
#include "driverlib/adc.h"
#include "driverlib/uart.h" // Allows for ease of use of UART

#include "grlib/grlib.h" // Header file inclusion for the graphics library

#include "drivers/cfal96x64x16.h" // Header file for OLED display

#define blinkyOnPeriod 100000 // defines how long the LED will stay lit
#define blinkyOffPeriod 100000 // defines how long the LED will remain off

//******************************************************************************
//
// Globals
//
//******************************************************************************
int whileLoop = 1;// Maintains indefinite while loop unless program exits
int32_t blinkyHandler = 1;// Maintains LED 'heartbeat' unless specified otherwise 
int32_t local_char; // Keeps the character input into PuTTy 

tContext sContext; // OLED drawing contextual structuring
tRectangle sRect; // Rectangle parameters for banner structuring

//******************************************************************************
//
// Function Declarations
//
//******************************************************************************
void splash(void); // splash screen display function
void blinky(void); // "Heartbeat" function
void clear(void); // clear the PuTTy window
void initializations(void); // Sets-up the software and hardware for usage
void printMenu(void); // re-prints the menu options to PuTTy
void putString(char *str); // prints a string to the OLED
void menuSwitch(void); // Switches between menu options depending on the input

int main(void) {
  //
  // Enable lazy stacking for interrupt handlers.  This allows floating-point
  // instructions to be used within interrupt handlers, but at the expense of
  // extra stack usage.
  //
  FPULazyStackingEnable();
  
  // Setting the clock to run directly from the crystal.
  SysCtlClockSet(SYSCTL_SYSDIV_1 | SYSCTL_USE_OSC | SYSCTL_OSC_MAIN | SYSCTL_XTAL_16MHZ);
  
  //
  // Calling the initial functions to set up TM4C123G, its peripherals,
  // OLED and clear PuTTy window for menu output.
  //
  initializations();
  splash(); // Prints out the splash screen to OLED.
  clear(); // Clears the PuTTy window for neatness.
  printMenu(); // Prints the UART menu for user IO.
  
  // Fill the part of the screen defined below to create a banner.
  sRect.i16XMin = 0;
  sRect.i16YMin = 0; 
  sRect.i16XMax = GrContextDpyWidthGet(&sContext) - 1; 
  sRect.i16YMax = 9; 
  GrContextForegroundSet(&sContext, ClrOrange);	
  GrRectFill(&sContext, &sRect); 
  GrContextForegroundSet(&sContext, ClrWhite);
  GrStringDrawCentered(&sContext, "Round & Round", -1, GrContextDpyWidthGet(&sContext)
						/ 2, 4, 0);
  
  IntMasterEnable(); // Enables Interrupts
  
  //
  // The main while loop the function will stay in unless acted upon by the
  // user through UART or an interrupt timer is being serviced.
  //
  while(whileLoop != 0) {				
	// Calling the 'heartbeat' function if specified to do so.
    if(blinkyHandler != 0) {
      blinky();
      blinkyHandler++;
    }
    whileLoop++;
    
    //
    // Checking to see if the user has input a character through UART and 
    // if so acting accordingly.
    while(UARTCharsAvail(UART0_BASE)) {
      local_char = UARTCharGetNonBlocking(UART0_BASE);
      menuSwitch();
    }		
 
	}
  } 
} 

//*****************************************************************************
//
// The error routine that is called if the driver library encounters an error.
//
//*****************************************************************************
#ifdef DEBUG
void __error__(char *pcFilename, uint32_t ui32Line) {}
#endif

//*****************************************************************************
//
// The UART interrupt handler.
//
//*****************************************************************************
void UARTIntHandler(void){
  uint32_t ui32Status;
  ui32Status = UARTIntStatus(UART0_BASE, true); // Get the interrupt status.
  UARTIntClear(UART0_BASE, ui32Status); // Clear the interrupt for UART
  // Loop while there are characters in the receive FIFO.
  while(UARTCharsAvail(UART0_BASE)) {
    // Read the next character from the UART and write it back to the UART.
    local_char = UARTCharGetNonBlocking(UART0_BASE);
    menuSwitch();
  }
}

//*****************************************************************************
//
// The function that is called when the program is first executed to set up
// the TM4C123G board, the peripherals, the timers, and the interrupts.
//
//*****************************************************************************
void initializations(void) {
  IntMasterDisable(); // Disable processor interrupts for configurations.
  
  //****************************************************************************
  //                                 LED
  //****************************************************************************
  SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOG); // Enable GPIO G usage.
  
  // Check if the LED peripheral access is enabled and wait if not.
  while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOG)) {}
  
  GPIOPinTypeGPIOOutput(GPIO_PORTG_BASE, GPIO_PIN_2); // GPIO output is pin 2.
  
  //****************************************************************************
  //                                 OLED
  //****************************************************************************
  CFAL96x64x16Init(); // Initialize the OLED display driver.
  GrContextInit(&sContext, &g_sCFAL96x64x16); // Initialize OLED graphics
  GrContextFontSet(&sContext, g_psFontFixed6x8); // Fix the font type
  
  //****************************************************************************
  //                                 UART
  //****************************************************************************
  SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0); // Enable UART 0 usage.
  while(!SysCtlPeripheralReady(SYSCTL_PERIPH_UART0)) {}; // wait until UART 0 is ready
  SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA); // Enable GPIO A usage.
  while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOA)) {}; // Wait until GPIO A is ready
  GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1); // Set Pins A0 and A1 for UART
  
  // Configure UART for 115200 baud rate, 8 in 1 operation
  UARTConfigSetExpClk(UART0_BASE, SysCtlClockGet(), 115200, 
					  (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE));
    
  IntEnable(INT_UART0); // Enable the interrupt for UART 0
  UARTIntEnable(UART0_BASE, UART_INT_RX | UART_INT_RT);  // Enable specific UART interrupt usage
  
  //****************************************************************************
  //                                  ADC
  //****************************************************************************
  SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0); // Enable ADC 0 usage.
  while(!SysCtlPeripheralReady(SYSCTL_PERIPH_ADC0)) {}; // Wait until ADC0 is ready
  SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD); // Enable GPIO port D usage.
  while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOD)) {}; // Wait until GPIO D is ready
  
  GPIOPinTypeADC(GPIO_PORTD_BASE, GPIO_PIN_7); // GPIO using pin 7 ADC usage
  ADCSequenceDisable(ADC0_BASE, 3); // Disable ADC sequence 3 to configure
  
  // Configuring the ADC
  ADCSequenceStepConfigure(ADC0_BASE, 3, ADC_TRIGGER_PROCESSOR, 0);
  ADCSequenceStepConfigure(ADC0_BASE, 3, 0, ADC_CTL_CH4 | ADC_CTL_IE | ADC_CTL_END);
  
  // Enabling the ADC for usage again
  ADCSequenceEnable(ADC0_BASE, 3);
  ADCIntClear(ADC0_BASE, 3);
  
  //****************************************************************************
  //                            GPIO OUT SIGNAL
  //****************************************************************************
  SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOL); // Enable GPIO Port L for usage
  while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOL)) {} // Wait for port to be ready
  GPIOPinTypeGPIOOutput(GPIO_PORTL_BASE, GPIO_PIN_0); // Enable GPIO PL0 for use
  GPIOPinTypeGPIOOutput(GPIO_PORTL_BASE, GPIO_PIN_1); // Enable GPIO PL1 for use
  GPIOPinTypeGPIOOutput(GPIO_PORTL_BASE, GPIO_PIN_2); // Enable GPIO PL2 for use
  GPIOPinTypeGPIOOutput(GPIO_PORTL_BASE, GPIO_PIN_3); // Enable GPIO PL3 for use
  
  //****************************************************************************
  //                              MISCELLANEOUS
  //****************************************************************************
  clear(); // Clear any outputs or inputs from the PuTTY window.
  GrFlush(&sContext); // Flush any cached operations
}

//*****************************************************************************
//
// PuTTY window clearing function.
//
//*****************************************************************************
void clear() { UARTCharPut(UART0_BASE, 12); }

//*****************************************************************************
//
// Using the character output function as a base for a parent function
// used to output an entire string to the OLED one character at a time.
//
//*****************************************************************************
void putString(char *str) {
  for(int i = 0; i < strlen(str); i++) {
    UARTCharPut(UART0_BASE, str[i]);
  }
}

//*****************************************************************************
//
// Print menu function that takes the complete menu as a string and 
// utilizes the print string function created above to output the entire menu
// in one transmission block to the PuTTY window.
//
//*****************************************************************************
void printMenu() {
  char*menu = "\rMenu Selection: \n\rC - Erase Terminal Window\n\rF - Follow Mode\n\rL 
			   - Flash LED\n\rM - Print the Menu\n\rN - Normal Mode\n\rQ - Quit this 
			   program\n\rR - Reverse Mode\n\rS - Stop Mode\n\n\r+ - Increase Speed\n\r-
			   - Decrease Speed\n\r";
  putString(menu);
}

//*********************************************************************
//
// If the input character is not invalid, begin the character matching
// statment below through the switch statment and act accordingly to
// the user input.
//
//*********************************************************************
void menuSwitch() {
  if (local_char != -1) { // Run only if the character in PuTTy is valid
    UARTCharPut(UART0_BASE, local_char); // Send a character to UART.
   
    // Begin character input matching to menu option.
    switch(local_char) {
    case 'C': // Clear PuTTY window 
      clear();
      break;
      
    case 'F':
      firstCycle = true;
      mode = 2;
      break;
      
    case 'L': //LED toggle 
      if(blinkyHandler == 0)
        blinkyHandler = 1;
      else
        blinkyHandler = 0;
      GPIOPinWrite(GPIO_PORTG_BASE, GPIO_PIN_2, 0);
      break;
      
    case 'M': // Re-print menu 
      UARTCharPut(UART0_BASE, 5);
      printMenu();
      break;
      
    case 'N':
        mode = 1;
      break;
      
    case 'Q': // Quit program
      IntMasterDisable();
      putString("\n\rBYE!"); // Goodbye message to PuTTy
      sRect.i16XMin = 0;
      sRect.i16YMin = 0;
      sRect.i16XMax = GrContextDpyWidthGet(&sContext) - 1;
      sRect.i16YMax = GrContextDpyHeightGet(&sContext) - 1;
      GrContextForegroundSet(&sContext, ClrBlack);
      GrContextBackgroundSet(&sContext, ClrBlack);
      GrRectFill(&sContext, &sRect);
      GrContextForegroundSet(&sContext, ClrRed);
      GrContextFontSet(&sContext, g_psFontFixed6x8);
      GrStringDrawCentered(&sContext, "Goodbye", -1, GrContextDpyWidthGet(&sContext) 
							/ 2, 30, false); // Goodbye message to OLED in red.
	  // If the user said to quit the whileLoop will NO LONGER be able to be ran		
      whileLoop = 0; 
      break;   
      
    default:
      char invalid[25] = "\n\rInvalid. Try Again: ";
      char*ptr = invalid;
      putString(ptr);
      break;
    } 
  }
}

//*****************************************************************************
//
// Blinky LED "heartbeat" function.
//
//*****************************************************************************
void blinky() {
  if(blinkyHandler == blinkyOnPeriod) {
    GPIOPinWrite(GPIO_PORTG_BASE, GPIO_PIN_2, GPIO_PIN_2);
    blinkyHandler = -blinkyOffPeriod;
  }
  
  if(blinkyHandler == -1) {
    GPIOPinWrite(GPIO_PORTG_BASE, GPIO_PIN_2, 0);
    blinkyHandler = 1;
  }
}

//*****************************************************************************
//
// Splash Screen
//
//*****************************************************************************
void splash() {
  // splash screen display parameters
  tRectangle screen;
  screen.i16XMin = 0; 
  screen.i16XMax = 96; // Maximum width of the OLED
  screen.i16YMin = 0;
  screen.i16YMax = 64; // Maximum height of the OLED
  
  // clear the screen from any reminaing displays 
  GrContextForegroundSet(&sContext, ClrBlack);
  GrRectFill(&sContext, &screen);
  
  // loading block parameters
  for(int length = 10; length <= 86; length++) {
    tRectangle loading;
    loading.i16XMin = 9;
    loading.i16XMax = length;
    loading.i16YMin = 26;
    loading.i16YMax = 39;
    GrContextForegroundSet(&sContext, ClrSalmon);
    if (length%10 == 0) {
      GrStringDrawCentered(&sContext, "Loading.  ", 11, 48, 20, true);
    }
    else if (length%10 == 1) {
      GrStringDrawCentered(&sContext, "Loading.. ", 11, 48, 20, true);
    }
    else if (length%10 == 2) {
      GrStringDrawCentered(&sContext, "Loading...", 11, 48, 20, true);
    }
    else {
      GrStringDrawCentered(&sContext, "Loading   ", 11, 48, 20, true);
    }
    SysCtlDelay(150000);
    GrContextForegroundSet(&sContext, ClrDeepSkyBlue);
    GrRectFill(&sContext, &loading);
  }
  
  // clear the screen so that it is set for the main screen
  GrContextForegroundSet(&sContext, ClrBlack);
  GrRectFill(&sContext, &screen);
}