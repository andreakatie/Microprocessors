//***************************************************************************** 
//
// Copyright (c) 2011-2017 Texas Instruments Incorporated. 
//
// This is part of revision 2.1.4.178 of the DK-TM4C123G Firmware Package.
//
//*****************************************************************************
#include <stdio.h> // Allows for use of inputs and outputs
#include "inc/hw_ints.h" // Inclusion of interrupt header file
#include "inc/hw_memmap.h" // Inclusion of device memory map header file
#include "inc/hw_types.h" // Inclusion of common hardware type header file
#include "driverlib/debug.h" // Allows for ease of debugging practices
#include "driverlib/gpio.h" // Allows for ease of use for GPIO
#include "driverlib/interrupt.h" // Allows for ease of use of interrupts
#include "driverlib/sysctl.h" // System Control header file
#include "driverlib/timer.h" // Header file inclusion for timing usage
#include "grlib/grlib.h" // Header file inclusion for the graphics library
#include "drivers/cfal96x64x16.h" // Header file for OLED display

int mode = 0; // Tells the code which mode the uesr wants to initiate
int lastRequested = -1; // Declaring a place holder for the last value requested
int index = 0; // The index of the code that is written to the GPIO pin
int speed = 200; // Speed of the Stepper Motor
int rpm = 60; // Revolutions per minute of the stepper motor

tContext sContext; // OLED drawing contextual structuring
tRectangle sRect; // Rectangle parameters for banner structuring

void initializations(void); // Sets-up the software and hardware for usage
void stop(void); // Stops the stepper motor from moving
void moveOnce(int move); // Moves the motor once in the direction desired

#ifdef DEBUG
void __error__(char *pcFilename, uint32_t ui32Line) {}
#endif

int main(void) {
  FPULazyStackingEnable();
  SysCtlClockSet(SYSCTL_SYSDIV_1 | SYSCTL_USE_OSC | SYSCTL_OSC_MAIN | SYSCTL_XTAL_16MHZ);
  initializations();
  
  sRect.i16XMin = 0;
  sRect.i16YMin = 0;
  sRect.i16XMax = GrContextDpyWidthGet(&sContext) - 1;
  sRect.i16YMax = GrContextDpyHeightGet(&sContext) - 1;
  GrContextForegroundSet(&sContext, ClrBlack);
  GrContextBackgroundSet(&sContext, ClrBlack);
  GrRectFill(&sContext, &sRect);
  GrContextForegroundSet(&sContext, ClrRed);
  GrContextFontSet(&sContext, g_psFontFixed6x8);
  GrStringDrawCentered(&sContext, "A song for you.", -1, GrContextDpyWidthGet(&sContext) / 2, 30, false);
  
  IntMasterEnable(); 
  
}  


void Timer0IntHandler(void) {
  TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT); 
  
}

void initializations(void) {
  IntMasterDisable(); // Disable processor interrupts for configurations.
  
  //****************************************************************************
  //                                 OLED
  //****************************************************************************
  CFAL96x64x16Init(); // Initialize the OLED display driver.
  GrContextInit(&sContext, &g_sCFAL96x64x16); // Initialize OLED graphics
  GrContextFontSet(&sContext, g_psFontFixed6x8); // Fix the font type
  
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
}

void stop(void) { 
  GPIOPinWrite(GPIO_PORTL_BASE, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3, 0);
}

void moveOnce(int move) {
  unsigned char pinWrite[4] = {0x6, 0xC, 0x9, 0x3};
  if(move==1) {
    if (index == 3) {
      index = 0;
    }
    else {
      index++;
    }
    GPIOPinWrite(GPIO_PORTL_BASE, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3, pinWrite[index]);
  }
  if (move == 0) {
    if (index == 0) {
      index = 3;
    }
    else {
      index--;
    }
    GPIOPinWrite(GPIO_PORTL_BASE, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3, pinWrite[index]);
  }
}