//*****************************************************************************
//
// CS322.50 Labratory 2 Software File
// Developed by: Andrea Gray and Daniel Pietz (c)
// Version: 1.40 28-JAN-2019
//
//*****************************************************************************

//*****************************************************************************
//
// uart_echo.c - Example source for reading data from and writing data to the 
// UART in an interrupt driven fashion for Labratory Exercise 2. 
//
// Copyright (c) 2011-2017 Texas Instruments Incorporated. 
//
// This is part of revision 2.1.4.178 of the DK-TM4C123G Firmware Package.
//
//*****************************************************************************

#include <stdint.h>				// Standard library header for  
						// integers with varying widths
#include <stdio.h> 			        // Standard library header for 
						// input and outputs -- sprintf
						// in this program specifically 
#include "driverlib/adc.h"			// ADC driver files in the 
                                                // driver library
#include "utils/uartstdio.h"			// Header file for driver files
                                                // for UART I/O
#include <stdbool.h>				// Standard library header for 
						// boolean data types
#include <time.h>			        // Header file with four main
						// variable types for 
                                                // manipulating date and time 
						// information
#include <stdlib.h> 				// Standard C library header --
						// malloc in this program 
                                                // specifically 
#include <string.h>				// Header file for the use and 
						// manipulation of strings
#include "inc/hw_memmap.h" 			// Header file for BASE call use
#include "driverlib/debug.h"		        // TM4C123G debugging header 
                                                // file
#include "driverlib/gpio.h"			// Header file for all GPIO 
                                                // function calls
#include "driverlib/sysctl.h" 		        // Header file for System 
                                                // Control Specs
#include "driverlib/uart.h"			// Header file for UART function
                                                // calls
#include "grlib/grlib.h" 			// Header file for output calls
#include "drivers/cfal96x64x16.h" 	        // Header file for OLED display 
                                                // dimension specifications
#include "drivers/buttons.h" 		        // Header file for push-buttons 
                                                // counter
#define LEDon 20000                             // defines the on period of the 
                                                // LED in ms
#define LEDoff 380000                           // defines the off period of the
                                                // LED in ms
#define refreshRate 60000			// The refresh rate for splash
                                                // screen output

// ADC data display type
typedef enum {off, numeric, histogram, terminator} displayType;

//******************************************************************************
//
// Function Declarations
//
//******************************************************************************
void putString(char *str);
void clear();
void printMenu();
void blinky(volatile uint32_t ui32Loop);
void InitConsole(void);

//*****************************************************************************
//
// This example application utilizes the UART to echo text. All characters
// recoeved on the UART are transmitted back to the UART.
//
//*****************************************************************************

//*****************************************************************************
//
// The error routine that is called if the driver library encounters an error.
//
//*****************************************************************************
#ifdef DEBUG
void __error__(char *pcFilename, uint32_t ui32Line) {
}
#endif

//*****************************************************************************
//
// Holds the current, debounced state of each button.  0 = pressed.
// We assume that we start with all the buttons released (though if one is
// pressed when the application starts, this will be detected).
//
//*****************************************************************************
static uint8_t g_ui8ButtonStates = ALL_BUTTONS;

//*****************************************************************************
//
// Initializes the GPIO pins used by the board pushbuttons with a weak 
// pull-up.
//
//*****************************************************************************
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

//*****************************************************************************
//
// Send a string to the UART.
//
//*****************************************************************************
void UARTSend(const uint8_t *pui8Buffer, uint32_t ui32Count) {
  //
  // Loop while there are more characters to send.
  //
  while(ui32Count--) {
    //
    // Write the next character to the UART.
    //
    UARTCharPutNonBlocking(UART0_BASE, *pui8Buffer++);
  }
}

//*****************************************************************************
//
// The main function to intialize the UART, LED, OLED, and run through the 
// program
//
//*****************************************************************************

int main(void) {
  tRectangle sRect;			        // OLED rectangle variable
  tContext sContext;				// OLED graphics buffer
  int gNumCharRecv = 0;				// Count for characters
						// recieved by the UART 
  volatile uint32_t ui32Loop;		        // Blinky LED volatile	
						// loop.
  bool shouldFlood = false;			// "Flood" character toggle
  bool shouldBlink = true;			// LED blinky toggle
  
  // positional information useed to animate the splash screen
  int16_t xValLast = 0;                         
  int16_t yValLast = 38;
  int16_t xVal = 0;                                                     
  int32_t yVal = 38;

  volatile int16_t val[3];                      // volatile variable to store
                                                // potentiometer data
  volatile uint32_t uiLoop;			// refresh rate increment
                                                // variable
  
  //*************************************************************************
  // Counter variables
  //*************************************************************************
  int tickCount = 0;				// Clock ticks for flood
						// character.
  int whileLoop = 1;				// Looping through the main
                                                // infinite while loop
  int colorSwitch = 0;				// OLED Color toggle
  int shouldCycle = 0;				// Boolean for Party Mode
  uint32_t buttonState = 0;			// Boolean to check if a
						// button was pressed.
  int buttonCounter = 0;			// Counting how many times 
						// a button was pressed.
  int lastPressed = 0;				// Saves the number of the
						// last button pressed for the 
						// OLED output.
  
  //*****************************************************************************
  //
  // Configure ADC0 for a single-ended input and a single sample.  Once the
  // sample is ready, an interrupt flag will be set.  Using a polling method,
  // the data will be read then displayed on the console via UART0.
  //
  //***************************************************************************** 
  
  // This array is used for storing the data read from the ADC FIFO. This 
  // project uses sequence 1 which has a FIFO depth of 4.  
  uint32_t pui32ADC0Value[4];

  // For this example ADC0 is used with AIN0 on port E7.
  // GPIO port D needs to be enabled so these pins can be used.
  SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0);
  SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);
  
  // Selecting the analog ADC function for pins 4 5 and 6.
  GPIOPinTypeADC(GPIO_PORTD_BASE, GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6);
  
  // Enable sequence 1 with a processor signal trigger.  Sequence 1
  // will do a single sample when the processor sends a signal to start the
  // conversion.
  ADCSequenceConfigure(ADC0_BASE, 1, ADC_TRIGGER_PROCESSOR, 0);
  
  // Configure step 0, 1, and 2 on sequence 1.  Sample channels 4, 5, and 6 in
  // single-ended mode (default). Tell the ADC logic that channel 6 is the 
  // last conversion on sequence 1 (ADC_CTL_END).  Sequence 1 has 4 steps.
  ADCSequenceStepConfigure(ADC0_BASE, 1, 0, ADC_CTL_CH4);
  ADCSequenceStepConfigure(ADC0_BASE, 1, 1, ADC_CTL_CH5);
  ADCSequenceStepConfigure(ADC0_BASE, 1, 2, ADC_CTL_CH6 | ADC_CTL_IE |
                           ADC_CTL_END);
  
  // Since sample sequence 1 is now configured, it must be enabled.
  ADCSequenceEnable(ADC0_BASE, 1);
  
  //*************************************************************************
  //
  // Set the clocking to run directly from the crystal.
  //
  //*************************************************************************
  SysCtlClockSet(SYSCTL_SYSDIV_1 | SYSCTL_USE_OSC | SYSCTL_OSC_MAIN |
                 SYSCTL_XTAL_16MHZ);
  
  //*************************************************************************
  //
  // Enable the GPIO port that is used for the on-board LED.
  //
  //*************************************************************************
  SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOG);
  
  //*************************************************************************
  //
  // Check if the LED peripheral access is enabled. If it is not, wait inside
  // the loop until it is. 
  //
  //*************************************************************************
  while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOG)) {
  }
  
  //*************************************************************************
  //
  // Enable the GPIO pin 2 for the LED.  Set the direction as output, and
  // enable the GPIO pin for digital function.
  //
  //*************************************************************************
  GPIOPinTypeGPIOOutput(GPIO_PORTG_BASE, GPIO_PIN_2);
  
  
  //*************************************************************************
  //
  // Initialize the OLED display driver.
  //
  //*************************************************************************
  CFAL96x64x16Init();
  
  //*************************************************************************
  //
  // Initialize the OLED graphics context.
  //
  //*************************************************************************
  GrContextInit(&sContext, &g_sCFAL96x64x16);
  
  
  //*************************************************************************
  //
  // Enable the peripherals used by this example.
  //
  //*************************************************************************
  SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);	// GPIO Pin Set
  SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);	// UART0 Set
  
  
  //*************************************************************************
  //
  // Set GPIO 0 and 1 as UART pins.
  //
  //*************************************************************************
  GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);
  
  //*************************************************************************
  //
  // Configure the UART for custom 115,200 baud rate.
  //
  //*************************************************************************
  UARTConfigSetExpClk(UART0_BASE, SysCtlClockGet(), 115200,
                      (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE |
                       UART_CONFIG_PAR_NONE));
  
  
  //*************************************************************************
  //
  // Check to see if the GPIO peripheral is ready. If it is not, wait inside
  // the loop until it is.
  //
  //*************************************************************************
  SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOG);
  while(!SysCtlPeripheralReady(SYSCTL_PERIPH_GPIOG)) {
  }
  
  //*************************************************************************
  //
  // Set the LED Pin output to pin 2. 
  //
  //*************************************************************************
  GPIOPinTypeGPIOOutput(GPIO_PORTG_BASE, GPIO_PIN_2); 
  
  ButtonsInit();				// Enable buttons for 
						// each loop iteration.
  int Looper = 0;				// Set loop variable and 
						// initialize to 0.
  clear();                                      // calling the clear function
                                                // to clear any remaining 
                                                // characters in text window
  printMenu();		                        // calling the print function
                                                // for outputting the menu
                                                // to the interface screen
  
  // Setting the OLED window boundaires
  sRect.i16XMin = 0;				
  sRect.i16YMin = 0;				
  sRect.i16XMax = GrContextDpyWidthGet(&sContext);	
  sRect.i16YMax = GrContextDpyHeightGet(&sContext);
  GrContextForegroundSet(&sContext, ClrBlack);  // Set the banner color to
                                                // dark blue.
  GrRectFill(&sContext, &sRect);		// Output the dimensions and
                                                // fill the OLED with the 
                                                // banner.
  
  int jumpTick = -1;                            // Keeps track of splash screen
                                                // ball location in cycle.
  int Rad = 5;                                  // Radius of the ball in pixels.
  int color = 0;                                // Color variable for the ball.
  
  //*************************************************************************
  //
  // Infinite While Loop 
  //
  //*************************************************************************
  while(1)									
  {
    // Reading the state of the button.
    buttonState = (GPIOPinRead(BUTTONS_GPIO_BASE,ALL_BUTTONS));
    // The button codes are : 30-UP, 29-DOWN, 27-LEFT, 23-RIGHT, and 15-SELECT
    xVal+=2;
    if(jumpTick == -1) {
      jumpTick = 0;
    }
    
    if(jumpTick != -1) {
      yVal = 38 - (10*jumpTick-jumpTick*jumpTick);
      jumpTick++;
      if(jumpTick > 10) {
        jumpTick = -1;
      }
    }
	
    GrContextInit(&sContext, &g_sCFAL96x64x16);	// Resets OLED output context.
    
    // Switch case for the color of an output to the OLED.
    // Key: 0-Blue, 1-Red, 2-Green, 3-Black
    switch(color) {
    case 0:
      GrContextForegroundSet(&sContext, ClrBlue);
      break;
    case 1:
      GrContextForegroundSet(&sContext, ClrRed);
      break;
    case 2:
      GrContextForegroundSet(&sContext, ClrGreen);
      break;
    case 3:
      GrContextForegroundSet(&sContext, ClrBlack);
      // Resetting the parameters of the OELD window
      sRect.i16XMin = 0;			
      sRect.i16YMin = 0;			
      sRect.i16XMax = xVal;
      sRect.i16YMax = GrContextDpyHeightGet(&sContext);
      GrRectFill(&sContext, &sRect);
      break;
    } 
	
    // Filling in the splash screen ball OLED output
    GrCircleFill(&sContext, xValLast,yValLast, Rad);
    GrContextForegroundSet(&sContext, ClrWhite); 
    GrCircleFill(&sContext, xVal,yVal, Rad);
    GrContextFontSet(&sContext, g_psFontCm12/*g_psFontFixed6x8*/);  
    GrFlush(&sContext);
    xValLast = xVal;
    yValLast = yVal;
    if(xVal >= 106) {                           // If the ball comes to the 
                                                // edge of the OLED window
                                                // parameters, then go back to
                                                // start of screen and change
                                                // ball color.
      xVal = -13;
      color++;
    }
    
    // If the ball reaches the last color then quit the splash screen.
    if(color == 4) {
      break;
    }
    
    // Waiting for refresh rate to be met so that the splash screen does not 
    // go through too fast or too slow. 
    for(uiLoop = 0; uiLoop < refreshRate; uiLoop++) {
    }
  }
  
  //*************************************************************************
  //
  // Fill the top part of the screen in the parameters below with dark blue to 
  // create the banner.
  //
  //*************************************************************************
  sRect.i16XMin = 0;
  sRect.i16YMin = 0;						
  sRect.i16XMax = GrContextDpyWidthGet(&sContext) - 1;	
  sRect.i16YMax = 9;						
  GrContextForegroundSet(&sContext, ClrDarkBlue);		
  GrRectFill(&sContext, &sRect);	
  
  //*************************************************************************
  //
  // Change foreground for white text.
  //
  //*************************************************************************
  GrContextForegroundSet(&sContext, ClrWhite);
  
  //*************************************************************************
  //
  // Put the application name in the middle of the banner.
  //
  //*************************************************************************
  GrContextFontSet(&sContext, g_psFontFixed6x8);
  GrStringDrawCentered(&sContext, "Gray & Pietz", -1,
                       GrContextDpyWidthGet(&sContext) / 2, 4, 0);
  
  //*************************************************************************
  //
  // Initialization of potentiometer values and display types.
  //
  //*************************************************************************
  val[0] = 0;
  val[1] = 0;
  val[2] = 0;
  displayType aDisp[3];
  aDisp[0] = off;
  aDisp[1] = off;
  aDisp[2] = off;
  
  //*************************************************************************
  //
  // Main infinite while loop used for ADC and I/O
  //
  //*************************************************************************
  while(whileLoop != 0) {				
    
    ADCProcessorTrigger(ADC0_BASE,1);           // Triggers a read from the ADC.
    while(!ADCIntStatus(ADC0_BASE,1,false)) {   // Waiting for an input.
    }
    ADCIntClear(ADC0_BASE,1);                   // Clear the input port.
    // Getting the values from the port.
    ADCSequenceDataGet(ADC0_BASE, 1, pui32ADC0Value);
    
    // Variable incrementations for every loop iteration.
    Looper++;
    whileLoop++;
    tickCount++;
    
    //*************************************************************************
    //
    // Setting the potentiometer values to display ADC values to fit in the 
    // range that the OLED can handle. 
    //
    //*************************************************************************
    val[0] = ((96*pui32ADC0Value[0])/4095);
    val[1] = ((96*pui32ADC0Value[1])/4095);
    val[2] = ((96*pui32ADC0Value[2])/4095);
    
    //*************************************************************************
    //
    // Flood character output character and timing.
    //
    //*************************************************************************
    if(tickCount == 3) {
      tickCount = 0;
      if(shouldFlood == 1) {
        UARTCharPut(UART0_BASE, '@');
      }
    }
    
    //*************************************************************************
    //
    // Blinky function toggle.
    //
    //*************************************************************************
    if(shouldBlink) {
      blinky(ui32Loop);
    }
    
    //*************************************************************************
    //
    // Checking for a button press, displaying output as designated, and 
    // incrementing counts accordingly.
    //
    //*************************************************************************
    buttonState = (~GPIOPinRead(BUTTONS_GPIO_BASE,ALL_BUTTONS) && ALL_BUTTONS);
    if(GPIOPinRead(BUTTONS_GPIO_BASE, ALL_BUTTONS)) {
      buttonCounter++;
      // Key: 1-UP, 2-DOWN, 3-LEFT, 4-RIGHT, 5-SELECT
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
    
    //*************************************************************************
    //
    // Changing the OLED output depending on Party Mode toggle and Banner Color 
    // menu selection.
    //
    //*************************************************************************
    if(shouldCycle == 1) {
      if(colorSwitch == 2) {
        colorSwitch = -1;	
      }
      
      // Setting OLED display as full avaliable rectangle output.
      colorSwitch++;
      sRect.i16XMin = 0;
      sRect.i16YMin = 0;
      sRect.i16XMax = GrContextDpyWidthGet(&sContext) - 1;
      sRect.i16YMax = GrContextDpyHeightGet(&sContext) - 1;
      
      // Switch case to determine the next OLED color output.
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
      
      // Re-draw the OLED rectangular output with above determined color
      // and correct banner message.
      GrRectFill(&sContext, &sRect);
      GrContextForegroundSet(&sContext, ClrWhite);
      GrContextFontSet(&sContext, g_psFontFixed6x8);
      GrStringDrawCentered(&sContext, "Gray & Pietz", -1,
                           GrContextDpyWidthGet(&sContext) / 2, 4, 0);
    }                                           // end shouldCycle()
    
    //*************************************************************************
    //
    // While loop when a character is inputted in the PuTTY window.
    //
    //*************************************************************************
    while(UARTCharsAvail(UART0_BASE)) {
      char str[50];				// Empty string declaration for
                                                // sprintf functions.
      int32_t local_char;			// Setting the input character
                                                // to a local variable.
      
      //*********************************************************************
      //
      // Re-draw the OLED with updated statistics
      //
      //*********************************************************************
      local_char = UARTCharGetNonBlocking(UART0_BASE);
      
      //*********************************************************************
      //
      // If the input character is not invalid, begin the character matching
      // statment below through the switch statment and act accordingly to
      // the user input.
      // 
      // Key: 67 'C' - Banner Color Switch, 69 'E' - Clear Interface Window,
      //      70 'F' - Flood Character Toggle, 76 'L' - LED Toggle,
      //      77 'M' - Reprint Menu, 80 'P' - Party Mode, 81 'Q' - Quit Program
      //
      //*********************************************************************
      if (local_char != -1) {
        gNumCharRecv++; 		        // Character input counter
        UARTCharPut(UART0_BASE, local_char);	// Sending a single character
                                                // through the UART_TX 	
                                                // (transmitting)channel

        switch(local_char) {
        case 67: 					
          if(colorSwitch == 2) {
            colorSwitch = -1;
          }
          colorSwitch++;			// Color cycle counter	
          
          // Specifying the banner area and recloration for menu options
          // C and P.
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
                               GrContextDpyWidthGet(&sContext) / 2, 4, 0);
          break;
          
        case 69: 			
          clear();
          break;
          
        case 70: 						
          if(shouldFlood == 0) {
            shouldFlood = 1;
          }
          else {
            shouldFlood = 0;
          }
          break;
          
        case 76: 				
          if(shouldBlink == 0)
            shouldBlink = 1;
          else
            shouldBlink = 0;
          break;
          
        case 77: 					
          printMenu();
          break;
          
        case 80:
          if(shouldCycle == 0) {
            shouldCycle = 1;
          }
          else
          {
            shouldCycle = 0;
          }
          break;
          
        case 81:
          putString("\n\rBYE!");		// Goodbye message to CPU 
                                                // window.
          
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
          GrStringDrawCentered(&sContext, "Goodbye", -1, GrContextDpyWidthGet(&sContext) / 2, 30, false);
          whileLoop = 0;
          break;         
        case 49:
          aDisp[0]++;
          break;
        case 50:
          aDisp[1]++;
          break;
        case 51:
          aDisp[2]++;
          break;
          
        } 					// End menu switch statment.
      }                                         // End valid character check.
    }		                                // End character scan loop.
    
    // Reset the value of each display type once it runs off the enum range.
    if(aDisp[0] == terminator) aDisp[0] = off;
    if(aDisp[1] == terminator) aDisp[1] = off;
    if(aDisp[2] == terminator) aDisp[2] = off;
    char tempStr[5];
    
    // Switch statement for ADC output 0 as a number, graph, or not displayed.
    switch(aDisp[0]) {
    case off:
      sRect.i16XMin = 0;						
      sRect.i16YMin = 16;						
      sRect.i16XMax = GrContextDpyWidthGet(&sContext);	
      sRect.i16YMax = 32;						
      GrContextForegroundSet(&sContext, ClrBlack);	
      GrRectFill(&sContext, &sRect);
      break;    
    case numeric:
      sRect.i16XMin = 0;					
      sRect.i16YMin = 16;				
      sRect.i16XMax = GrContextDpyWidthGet(&sContext);
      sRect.i16YMax = 32;						
      GrContextForegroundSet(&sContext, ClrBlack);			
      GrRectFill(&sContext, &sRect);
      GrContextForegroundSet(&sContext, ClrWhite);
      sprintf(tempStr,"%i",pui32ADC0Value[0]);
      GrStringDrawCentered(&sContext, tempStr, -1,
                           GrContextDpyWidthGet(&sContext) / 2, 24, 16);       
      break;
    case histogram:
      sRect.i16XMin = 0;				
      sRect.i16YMin = 16;					
      sRect.i16XMax = val[0];	
      sRect.i16YMax = 32;	
      GrContextForegroundSet(&sContext, ClrRed);	
      GrRectFill(&sContext, &sRect);
      sRect.i16XMin = val[0];	
      sRect.i16XMax = 96;					
      GrContextForegroundSet(&sContext, ClrBlack);	
      GrRectFill(&sContext, &sRect);
      break;  
    }
    
    // Switch statment for ADC output 1 as a number, graph, or not displayed.
    switch(aDisp[1]) {
    case off:
      sRect.i16XMin = 0;								
      sRect.i16YMin = 32;
      sRect.i16XMax = GrContextDpyWidthGet(&sContext);	
      sRect.i16YMax = 48;						
      GrContextForegroundSet(&sContext, ClrBlack);		
      GrRectFill(&sContext, &sRect);
      break;    
    case numeric:
      sRect.i16XMin = 0;			
      sRect.i16YMin = 32;					
      sRect.i16XMax = GrContextDpyWidthGet(&sContext);	
      sRect.i16YMax = 48;						
      GrContextForegroundSet(&sContext, ClrBlack);		
      GrRectFill(&sContext, &sRect);
      GrContextForegroundSet(&sContext, ClrWhite);
      sprintf(tempStr,"%i",pui32ADC0Value[1]);
      GrStringDrawCentered(&sContext, tempStr, -1,
                           GrContextDpyWidthGet(&sContext) / 2, 40, 16);       
      break;
    case histogram:
      sRect.i16XMin = 0;					
      sRect.i16YMin = 32;						
      sRect.i16XMax = val[1];
      sRect.i16YMax = 48;						
      GrContextForegroundSet(&sContext, ClrGreen);			
      GrRectFill(&sContext, &sRect);
      sRect.i16XMin = val[1];
      sRect.i16XMax = 96;
      GrContextForegroundSet(&sContext, ClrBlack);		
      GrRectFill(&sContext, &sRect);
      
      break;  
    }
    
    // Switch statment for ADC output 2 as a number, graph, or not displayed.
    switch(aDisp[2]) {
    case off:
      sRect.i16XMin = 0;			
      sRect.i16YMin = 48;					
      sRect.i16XMax = GrContextDpyWidthGet(&sContext);	
      sRect.i16YMax = 64;					
      GrContextForegroundSet(&sContext, ClrBlack);	
      GrRectFill(&sContext, &sRect);
      break;    
    case numeric:
      sRect.i16XMin = 0;						
      sRect.i16YMin = 48;					
      sRect.i16XMax = GrContextDpyWidthGet(&sContext);
      sRect.i16YMax = 64;						
      GrContextForegroundSet(&sContext, ClrBlack);			
      GrRectFill(&sContext, &sRect);
      GrContextForegroundSet(&sContext, ClrWhite);
      sprintf(tempStr,"%i",pui32ADC0Value[2]);
      GrStringDrawCentered(&sContext, tempStr, -1,
                           GrContextDpyWidthGet(&sContext) / 2, 56, 16);       
      break;
    case histogram:
      sRect.i16XMin = 0;						
      sRect.i16YMin = 48;				
      sRect.i16XMax = val[2];
      sRect.i16YMax = 64;					
      GrContextForegroundSet(&sContext, ClrDarkBlue);	
      GrRectFill(&sContext, &sRect);
      sRect.i16XMin = val[2];	
      sRect.i16XMax = 96;						
      GrContextForegroundSet(&sContext, ClrBlack);		
      GrRectFill(&sContext, &sRect);
      break;  
    }
  }                                             // End indefinite while()
} 						// End of main()

//*****************************************************************************
//
// PuTTY window clearing function.
//
//*****************************************************************************
void clear() {
  UARTCharPut(UART0_BASE, 12);
}

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
  char*menu = "\rMenu Selection: \n\rP - Party Mode\n\rC - Change Background Color\n\rE - Erase Terminal Window\n\rL - Flash LED\n\rF - Flood Character\n\rM - Print the Menu\n\r1- Toggle Display Mode for First Potentiometer\n\r2- Toggle Display Mode for Second Potentiometer\n\r3- Toggle Display Mode for Third Potentiometer\n\rQ - Quit this program\n\r";
  putString(menu);
}

//*****************************************************************************
//
// Blinky LED "heartbeat" function.
//
//*****************************************************************************
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