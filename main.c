#include <stdint.h>
#include <stm32l476xx.h>
#include "main.h"
/**
**************************************************************************************************
* @file main.c
* @brief Main program body
* @author: Justin Turner
* @version 2.0
**************************************************************************************************
*/

volatile uint32_t speed_level = ONE_HZ; //set default speed level
enum directions direction = LEFT; //sets default direction as LEFT

const uint32_t FLASHfrequency[] = {2, 4, 8, 16}; //HZ
volatile int32_t LEDcount = -1;//ensures that the rightmost LED turns on at program start
volatile uint32_t rollover_detected = 0; //flag that is set when the moving LED rolls off the line
volatile uint32_t msTimer = 0;

struct Light_Emitting_Diode LEDS[] = { //declaring the array of LEDS
		 {GPIOC, 8},  {GPIOC, 9}, {GPIOC, 6}, {GPIOB, 8},
		 {GPIOC, 5}, {GPIOB, 9},  {GPIOA, 12}, {GPIOA, 11},
		 {GPIOA, 6}, {GPIOB, 12}, {GPIOA, 7}, {GPIOB, 11},
		 {GPIOB, 6},  {GPIOC, 7}, {GPIOB, 2}, {GPIOA, 9},
};

//================================================================================================
// configure_LEDS()
//
// @parm:   *port = GPIOx
//			pins[] = Array containing the port pin numbers used
//          number_of_pins = Number of port pins used
//			port_clock_num = The number associated with the port clock "0 for port A"
// @return: none
//
// 		Conveniently configures multiple LEDs at once. Also assists with board wire management.
//================================================================================================
void configure_LEDS (GPIO_TypeDef *port, uint32_t pins[], uint32_t number_of_pins, uint32_t port_clock_num)
{
	RCC->AHB2ENR |= (0x1 << port_clock_num);
	for (uint32_t i = 0; i < number_of_pins; i++){
		port->MODER &= ~(0x3 << (2 * pins[i]));
		port->MODER |= (0x1 << (2 * pins[i]));
		port->OTYPER &= ~(0x1 << pins[i]);
		port->OSPEEDR &= ~(0x3 << (2 * pins[i]));
		port->PUPDR &= ~(0x3 << (2 * pins[i]));
	}
}
//================================================================================================
// configure_external_switches()
//
// @parm: none
// @return: none
//
// 		Setups 2 external push button switches at PA4 and PA1 for input. Also configures external
//		interrupts for pins 4 and 1
//
//  	Note: Left Button = PA4, Right Button = PA1
//================================================================================================
void configure_external_switches(void)
{
		//RCC->AHB2ENR |= (0x1 << 0); //Port A - already enabled in previous function
		GPIOA->MODER &= ~(0x3 << (2 *4 ));//input pin (PA4 = 00)
		GPIOA->PUPDR = (GPIOA->PUPDR &= ~(0x3 << (2*4)))|(0x1 << 2 * 4);//toggles pull-up (PA4=01)
		//configure switch #2 PA1 for input
		GPIOA->MODER &= ~(0x3 << (2 * 1));//input pin (PA1 = 00)
		GPIOA->PUPDR = (GPIOA->PUPDR &= ~(0x3 << (2*1)))|(0x1 << 2 * 1);//toggles pull-up (PA1=01)

	    RCC->APB2ENR |= (0x1 << 0); //Enable system configuration clock for external interrupts
		SYSCFG->EXTICR[0] |= (0x0 << 4);//setup PA1 as target pin for EXTI1
		SYSCFG->EXTICR[1] |= (0x0 << 0);//setup PA4 as target pin for EXTI4
     	EXTI -> FTSR1 |= (0x1 << 4)| (0x1 << 1); //enable falling edge trigger detection
     	EXTI -> IMR1 |= (0x1 << 4)|(0x1 << 1); //Unmask EXTI4 and EXTI1
    	NVIC_SetPriority(EXTI4_IRQn, 6); //pin 4 interrupt: priority level 6
    	NVIC_SetPriority(EXTI1_IRQn, 5);//pin 1 interrupt: priority level 5
    	NVIC_EnableIRQ(EXTI4_IRQn); //enable interrupt at pin 4
    	NVIC_EnableIRQ(EXTI1_IRQn); //enable interrupt at pin 1
}


//================================================================================================
// configureSysTickInterrupt()
//
// @parm: none
// @return: none
//
// 		Configures the hardware so the SysTick timer will trigger every 1ms
//================================================================================================
void configureSysTickInterrupt(void)
{
	SysTick->CTRL = 0; //disable SysTick timer
	NVIC_SetPriority(SysTick_IRQn, 7); //set priority level at 7
	SysTick->LOAD = 3999; //set the counter reload value 1ms
	SysTick->VAL = 0; //reset SysTick timer value
	SysTick->CTRL |= SysTick_CTRL_CLKSOURCE_Msk; //use system clock
	SysTick->CTRL |= SysTick_CTRL_TICKINT_Msk; //enable SysTick interrupts
}

//================================================================================================
// updateARR()
//
// @parm: ReloadValue = Value used in the ARR (determines frequency)
// @return: none
//
//================================================================================================
void updateARR (uint32_t ReloadValue){
	  TIM2->ARR = ((cntclk/ReloadValue) - 1); //update SysTick load register
	  TIM2->CNT = 0;
}

//================================================================================================
// configureTIM2()
//
// @parm: none
// @return: none
//
// 		Configures TIM2 to for interrupt generation
//      Note: The counter reset and ARR value is handled by the updateARR function
//================================================================================================
void configureTIM2 (void)
{
	  RCC->APB1ENR1 |= (1 << 0); // Enable TIM2 clock
	  TIM2->PSC = (SYS_CLK_FREQ/cntclk -1);
	  updateARR(FLASHfrequency[speed_level]);
	  TIM2->DIER |= (1 << 0); // Enable update interrupt
	  TIM2->CR1 |= (1 << 0);// Start timer
	  NVIC_SetPriority(TIM2_IRQn, 3); //set priority level at 3
	  NVIC_EnableIRQ(TIM2_IRQn);// Enable interrupt in NVIC
}


//================================================================================================
// LEFT_lightshow()
//
// @parm: LEDcount = Value that determines which LED is lit and which LED to turn off
// @return: none
//
// 		Makes sure the appropriate LED is turned on and that the previous led is also turned off.
//================================================================================================
void LEFT_Lightshow (uint32_t LEDcount){
	LEDS[LEDcount].port-> ODR |= (0x1 << LEDS[LEDcount].pin);//turn on the current LED
	//--calculate previous LED------------------------------------------------------------------------
	//if LEDcount > 15, prevLED = LEDcount - 1, else the LEDcount is 0 the prevLED is 15
	uint32_t prevLED = (LEDcount > 0)? LEDcount - 1: 15;
	LEDS[prevLED].port-> ODR &= ~(0x1 << LEDS[prevLED].pin); //turn off the previous LED




}
//================================================================================================
// RIGHT_lightshow()
//
// @parm: LEDcount = Value that determines which LED is lit and which LED to turn off
// @return: none
//
// 		Makes sure the appropriate LED is turned on and that the previous led is also turned off.
//================================================================================================
void RIGHT_Lightshow (uint32_t LEDcount){
	LEDS[LEDcount].port-> ODR |= (0x1 << LEDS[LEDcount].pin);//turn on the current LED
	//--calculate previous LED------------------------------------------------------------------------
	//if LEDcount < 15, prevLED = LEDcount + 1, else the LEDcount is 15 the prevLED is 0
	uint32_t prevLED = (LEDcount < 15)? LEDcount + 1: 0;
	LEDS[prevLED].port-> ODR &= ~(0x1 << LEDS[prevLED].pin); //turn off the previous LED

}


int main(void)
{
	//--initialize hardware------------------------------------------------------------------------
	uint32_t GPIOA_pins[] = {9,7,6,12,11}; //LEDs connected to port A's pins
	uint32_t GPIOB_pins[] = {8,9,12,11,6,2}; //LEDs connected to port B's pins
	uint32_t GPIOC_pins[] = {8,9,6,5,7};//LEDs connected to port C's pins

	configure_LEDS(GPIOA, GPIOA_pins, GPIOApins_used, 0);//configure port A's LEDs
	configure_LEDS(GPIOB, GPIOB_pins, GPIOBpins_used, 1);//configure port B's LEDs
	configure_LEDS(GPIOC, GPIOC_pins, GPIOCpins_used, 2);//configure port C's LEDs
	configure_external_switches();//configure the switches and their dedicated interrupts
	configureSysTickInterrupt();
	configureTIM2(); //configure general purpose TIM2
	startSysTickTimer_MACRO;


	//---------------------------------------------------------------------------------------------
	// Polling loop that continually checks the direction the LED is moving and if a roll over
	// has occurred.
	//---------------------------------------------------------------------------------------------


	while (1)
	{

		switch (direction){
		case LEFT:
			LEFT_Lightshow(LEDcount);
			break;
		case RIGHT:
			RIGHT_Lightshow(LEDcount);
			break;
		}
		if (rollover_detected){
			speed_level = (speed_level+1)%4;
			updateARR(FLASHfrequency[speed_level]);
			rollover_detected = 0;
		}


	}
}//end main

//================================================================================================
// EXTI4_IRQHandler()
//
// @parm: none
// @return: none

//================================================================================================
void EXTI4_IRQHandler(void)
{
	if (EXTI->PR1 & (0x1 << 4)) {//if the interrupt flag is set....
		EXTI->PR1 |= (0x1 << 4);  // Clear interrupt flag
		if(direction != LEFT){//if the LED is not already moving left....
			direction = LEFT; //change the direction to left
		    TIM2->EGR |= (1 << 0); // force update event /manual trigger
		}
	}
}

//================================================================================================
// EXTI1_IRQHandler()
//
// @parm: none
// @return: none
//
//
//================================================================================================
void EXTI1_IRQHandler(void)
{
	if (EXTI->PR1 & (0x1 << 1)) { //if the interrupt flag is set....
		EXTI->PR1 |= (0x1 << 1);  // Clear interrupt flag
		if(direction != RIGHT){//if the LED is not already moving right....
			direction = RIGHT;//change the direction to right
		    TIM2->EGR |= (1 << 0);  // force update event /manual trigger
		}
	}
}

//================================================================================================
// SysTick_Handler()
//
// @parm: none
// @return: none
//
// 		  Increments the global millisecond timer (msTimer).
//================================================================================================
void SysTick_Handler(void)
{
	msTimer++; //goes up every 1ms
}


//================================================================================================
// TIM2_IRQHandler()
//
// @parm: none
// @return: none
//
// 		Responsible for controlling the LEDcount variable: Increments or decrements it based on the
//		direction variable.
//================================================================================================
void TIM2_IRQHandler(void)
{
    if (TIM2->SR & (1 << 0)) {
        TIM2->SR &= ~(1 << 0);// Clear update flag
        switch (direction){
		case LEFT: //moving in a left direction
			LEDcount++; //increment
			if(LEDcount == 16){//if the led has rolled off the left side of the line....
				rollover_detected=1; //set rollover_detected flag
				LEDcount = 0;//reset LEDcount back to the rightmost LED
			}

			break;
		case RIGHT: //moving in a right direction
			LEDcount--; //decrement
			if(LEDcount == -1){ //if the led has rolled off the right side of the line....
				rollover_detected = 1; //set rollover_detected flag
				LEDcount = 15; //reset LEDcount back to the leftmost LED
			}
			break;
		}//end switch
    }
}












































