#include "device_driver.h"
#include <stdio.h>

extern volatile int Key_Pressed;

void _Invalid_ISR(void)
{
	unsigned int r = Macro_Extract_Area(SCB->ICSR, 0x1ff, 0);
	printf("\nInvalid_Exception: %d!\n", r);
	printf("Invalid_ISR: %d!\n", r - 16);
	for(;;);
}

void EXTI15_10_IRQHandler(void)
{
	// KEY Pressed 메시지 인쇄
	// KEY(EXTI) Pending Clear
	// NVIC Pending Clear

	Key_Pressed = 1;

	EXTI->PR = 0x1 << 13;
	NVIC_ClearPendingIRQ(40);
}
