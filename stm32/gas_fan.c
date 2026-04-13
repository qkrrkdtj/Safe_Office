#include "device_driver.h"
#include "gas_fan.h"

#define GASFAN_IN1_PIN	1u
#define GASFAN_IN2_PIN	2u
#define GASFAN_GPIO	GPIOB

void GasFan_Init(void)
{
	Macro_Set_Bit(RCC->AHB1ENR, 1);
	Macro_Write_Block(GASFAN_GPIO->MODER, 0x3u, 0x1u, GASFAN_IN1_PIN * 2u);
	Macro_Write_Block(GASFAN_GPIO->MODER, 0x3u, 0x1u, GASFAN_IN2_PIN * 2u);
	Macro_Clear_Bit(GASFAN_GPIO->OTYPER, GASFAN_IN1_PIN);
	Macro_Clear_Bit(GASFAN_GPIO->OTYPER, GASFAN_IN2_PIN);
	GasFan_Off();
}

void GasFan_On(void)
{
	Macro_Set_Bit(GASFAN_GPIO->ODR, GASFAN_IN1_PIN);
	Macro_Clear_Bit(GASFAN_GPIO->ODR, GASFAN_IN2_PIN);
}

void GasFan_Off(void)
{
	Macro_Clear_Bit(GASFAN_GPIO->ODR, GASFAN_IN1_PIN);
	Macro_Clear_Bit(GASFAN_GPIO->ODR, GASFAN_IN2_PIN);
}
