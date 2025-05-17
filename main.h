#ifndef MAIN_H
#define MAIN_H

#include <stdint.h>
#include "stm32l476xx.h"

/**
**************************************************************************************************
* @file main.h
* @brief Header file for program main
* @author Justin Turner
* @version Header for main.c module
* ------------------------------------------------------------------------------------------------
* Defines the constants, enums, structures, and function prototypes
**************************************************************************************************
*/
#define startSysTickTimer_MACRO (SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk)
#define RightButtonPressed ((!(GPIOA->IDR & (0x1 << 1))))//macro
#define LeftButtonPressed ((!(GPIOA->IDR & (0x1 << 4)))) //macro
#define SpecialButtonPressed ((!(GPIOC->IDR & (0x1 << 13))))//macro

#define NUM_of_LEDS 16

#define GPIOApins_used 5 //number of GPIOA pins used for LEDs
#define GPIOBpins_used 6 //number of GPIOB pins used for LEDs
#define GPIOCpins_used 5 //number of GPIOC pins used for LEDs

#define SYS_CLK_FREQ 4000000// default frequency of the device = 4 MHZ
#define cntclk 1000

#define startTIM2_MACRO (TIM2->CR1 |= (1 << 0)) //start timer
#define stopTIM2_MACRO (TIM2->CR1 &= ~(1 << 0)) //stop timer)
#define RightButtonPressed ((!(GPIOA->IDR & (0x1 << 1))))//macro
#define LeftButtonPressed ((!(GPIOA->IDR & (0x1 << 4)))) //macro


#define DEBOUNCE_DELAY 20//arbitrary value that provided an appropriate delay while still being responsive

//enums
enum SPEED { ONE_HZ = 0, TWO_HZ, FOUR_HZ, EIGHT_HZ, SIXTEEN_HZ };
enum directions { LEFT = 0, RIGHT = 1};


//Structures
struct Light_Emitting_Diode{
	GPIO_TypeDef *port; //GPIOx
	uint32_t pin; //Pin number
};

//Function Prototypes
void configure_LEDS(GPIO_TypeDef *port, uint32_t pins[], uint32_t number_of_pins, uint32_t port_clock_num);
void configure_external_switches(void);
void configureSysTickInterrupt(void);
void updateARR(uint32_t ReloadValue);
void configureTIM2(void);
void LEFT_Lightshow(uint32_t LEDcount);
void RIGHT_Lightshow(uint32_t LEDcount);

#endif /* MAIN_H */
