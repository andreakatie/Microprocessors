//*****************************************************************************
// File: Lab10.c
// Project: Lab10
// Author(s): Andrea Gray and Daniel Pietz
// Date Complete: April 23, 2019
//
// This application provides a connection from the TM4C board to the UART
// allowing the user to interact with the board via the keyboard. The
// application will also use an interrupt to send a values to the PWM to 
// generate proper note frequencies in order to play a song.
//
//*****************************************************************************

#include <stdint.h> // Defines uint32_t, uint8_t
#include <stdbool.h> // Defines boolean values
#include <string.h> // Used for strlen()
#include <stdio.h> // Used for sprintf()
#include <ctype.h> // Used for toupper()


#include "inc/hw_ints.h" // Used to define Hardware Interrupts
#include "inc/hw_memmap.h" // Used to include constants involved with memory locations
#include "inc/hw_types.h" // Used in the original timers.c example



#include "driverlib/debug.h"
#include "driverlib/fpu.h"
#include "driverlib/interrupt.h" // Defines the interrupt Enable/Disable funtions
#include "driverlib/sysctl.h" // Defines anything prefixed with SysCtl ot SYSCTL
#include "driverlib/timer.h" 
#include "driverlib/pin_map.h" // Defines GPIO_PA0_U0RX and TX for the UART
#include "driverlib/gpio.h"
#include "driverlib/uart.h"
#include "driverlib/pwm.h"

#include "grlib/grlib.h"
#include "drivers/cfal96x64x16.h"

// Helps with timing in the while(1) loop, creating ~1s LED cycle at 16MHZ
#define TIMING 800000

// Define load values to achieve correct note frequencies
#define F 45815
#define G 40816
#define Ab 38554
#define Bb 34323
#define C 30578
#define Db 28862
#define Eb 25713
#define Rest 0

//
// Global structure used to allow Timer0Int to affect main and Timer1Int
//
TOGGLES toggle = {1,0};

//
// Global to determine location in music array
//
uint32_t ui32NotePosition = 0;

//
// Global to hold the value of the total number of notes/beats in the music
//
uint32_t ui32TotalNotes = 97;

//
// Array to hold the notes to be played
//
uint32_t g_ui32Notes[97] = {Rest};

//
// Global to allow Interupts to call drawing/printing to screen functions
// without defining sContext in the interrupts every time
//
tContext Context;

//
// Function Prototypes
//
void PrintBlackScreen(void);
void PrintString(char buffer2[], uint8_t middle);
void EnableLED(void);
void UARTSend(const uint8_t message[]);
void SplashScreen(void);
void EnablePWM(void);
void ConfigureUART(void);
void UARTIntHandler(void);
void ReadChar(const int32_t local_char);
void PlayNote(void);

//*****************************************************************************
//
// The interrupt handler for the first timer interrupt.
//
//*****************************************************************************
void Timer0IntHandler(void) {
    // used to hold the character typed
    int32_t local_char;
    
    //
    // Clear the timer interrupt.
    //
    TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT);

    //
    // Play the next note
    //
    PlayNote();
    
    // 
    // Interpret what character was read
    //          
    while(UARTCharsAvail(UART0_BASE)) {
        local_char = UARTCharGetNonBlocking(UART0_BASE);
        if (local_char != -1) 
            menuSwitch();
    }
}

//*****************************************************************************
//
// This example application demonstrates the use of the timers to generate
// periodic interrupts.
//
//*****************************************************************************
int main(void) {    
    //
    // Enable lazy stacking for interrupt handlers.  This allows floating-point
    // instructions to be used within interrupt handlers, but at the expense of
    // extra stack usage.
    //
    FPULazyStackingEnable();

    //
    // Set the clocking to run directly from the crystal
    //
    SysCtlClockSet(SYSCTL_SYSDIV_1 | SYSCTL_USE_OSC | SYSCTL_OSC_MAIN | SYSCTL_XTAL_16MHZ);
  
    //
    // Initialize the graphics context and find the middle X coordinate.
    //
    GrContextInit(&g_sContext, &g_sCFAL96x64x16);

    
    //
    // Display Splash Screen
    //
    SplashScreen();
    
    //
    // Send Menu to UART and delay to display it
    //
    UARTSend(MENU);
    SysCtlDelay((SysCtlClockGet()*10)/30);

  
    IntMasterEnable(); 
    
    //
    // Loop while Exit key is not pressed
    //  
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
  }
}

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
      
    case 'R':
        mode = 3;
      break;
      
    case 'S':
      mode = 0;
      break;
      
    case '+':
      speed += 200;
      rpm += 60;
      TimerLoadSet(TIMER0_BASE, TIMER_A, SysCtlClockGet()/speed);
      break;
    
    case '-':
      speed -= 200;
      rpm -= 60;
      if (speed <= 0) {
        rpm = 0;
        speed = 60;
      }
      TimerLoadSet(TIMER0_BASE, TIMER_A, SysCtlClockGet()/speed);
      break;
      
    default:
      char invalid[25] = "\n\rInvalid. Try Again: ";
      char*ptr = invalid;
      putString(ptr);
      break;
    } 
  }
}

void PlayNote(void)
{
    //
    // If the note position is valid, play the note
    //
    if (g_ui32NotePosition < g_ui32TotalNotes)
    {
        // Disable The PWM generator to change values
        PWMGenDisable(PWM0_BASE, PWM_GEN_0);
      
        //
        // Set the period based on the intended note to be played
        //
        PWMGenPeriodSet(PWM0_BASE, PWM_GEN_0, g_ui32Notes[g_ui32NotePosition]);
        
        //
        // Set the pulse width of PWM0 for a 50% duty cycle.
        //
        PWMPulseWidthSet(PWM0_BASE, PWM_OUT_0, 
                              g_ui32Notes[g_ui32NotePosition] / 2);

        //
        // Set the pulse width of PWM1 for a 50% duty cycle.
        //
        PWMPulseWidthSet(PWM0_BASE, PWM_OUT_1, 
                              g_ui32Notes[g_ui32NotePosition] / 2);

        //
        // Start the timers in generator 0.
        //
        PWMGenEnable(PWM0_BASE, PWM_GEN_0);
        
        //
        // Increment the Note Position to play the next note
        //
        g_ui32NotePosition++;
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
  
    //
    // Enable GPIO Port H
    //
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOH);
  
    //
    // Enable PWM0
    //
    SysCtlPeripheralEnable(SYSCTL_PERIPH_PWM0);
    
    //
    // Assign Port D Pin 5 to the ADC
    //
    GPIOPinTypePWM(GPIO_PORTH_BASE, GPIO_PIN_0);
    
    //
    // Configure the GPIO Pin as PWM0
    //
    GPIOPinConfigure(GPIO_PH0_M0PWM0);
    
    //
    // Configure the PWM Generator
    //
    PWMGenConfigure(PWM0_BASE, PWM_GEN_0,
                    PWM_GEN_MODE_DOWN | PWM_GEN_MODE_NO_SYNC);   
    //
    // Enable the outputs.
    //
    PWMOutputState(PWM0_BASE, (PWM_OUT_0_BIT | PWM_OUT_1_BIT), true);
	
	    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);
  
  //****************************************************************************
  //                                 TIMERS
  //****************************************************************************
  SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0); // Enabling Timer 0 for use
   
  // Configuring the 32-bit periodic timer 0
  TimerConfigure(TIMER0_BASE, TIMER_CFG_PERIODIC);
  TimerLoadSet(TIMER0_BASE, TIMER_A, SysCtlClockGet()/speed);
  
  IntEnable(INT_TIMER0A); // Enabling timer 0 for interrupt usage
  TimerIntEnable(TIMER0_BASE, TIMER_TIMA_TIMEOUT); // Enabling timer 0 timeout
  TimerEnable(TIMER0_BASE, TIMER_A); // Enabling timer 0
  
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
  
    //
    // Configure the 32-bit periodic timers.
    //
    TimerConfigure(TIMER0_BASE, TIMER_CFG_PERIODIC);
    
    //
    // Load the Timer with a the value to achieve 120 BPM
    //
    TimerLoadSet(TIMER0_BASE, TIMER_A, 3692308);

    //
    // Setup the interrupts for the timer timeouts.
    //
    IntEnable(INT_TIMER0A);
    TimerIntEnable(TIMER0_BASE, TIMER_TIMA_TIMEOUT);

    //
    // Enable the timers.
    //
    TimerEnable(TIMER0_BASE, TIMER_A);

    //
    // Enable processor interrupts.
    //
}

//*****************************************************************************
//
// PuTTY window clearing function.
//
//*****************************************************************************
void clear() { UARTCharPut(UART0_BASE, 12); }