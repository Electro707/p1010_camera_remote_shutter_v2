#include "stm32g0xx.h"
#include "stm32g0xx_ll_rcc.h"
#include "stm32g0xx_ll_system.h"
#include "stm32g0xx_ll_utils.h"
#include "gc9a01.h"
#include "config.h"
#include <stddef.h>
#include <stdlib.h>
#include <math.h>

void line_draw_abstract(uint8_t *canvasBuff, uint16_t canvasW, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void line_draw_vert_abstract(uint8_t *canvasBuff, uint16_t canvasW, uint16_t x, uint16_t h);

void initMcu(void){
	// init clock, 32Mhz system and periferal clock
	LL_FLASH_SetLatency(LL_FLASH_LATENCY_1);
	while(LL_FLASH_GetLatency() != LL_FLASH_LATENCY_1);

	LL_RCC_HSI_Enable();
	while(LL_RCC_HSI_IsReady() != 1);

	LL_RCC_PLL_ConfigDomain_SYS(LL_RCC_PLLSOURCE_HSI, LL_RCC_PLLM_DIV_1, 8, LL_RCC_PLLR_DIV_2);
	LL_RCC_PLL_Enable();
	LL_RCC_PLL_EnableDomain_SYS();
	while(LL_RCC_PLL_IsReady() != 1);

	LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
	LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL);
	while(LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL);

	LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_1);
	LL_Init1msTick(64000000);
	LL_SetSystemCoreClock(64000000);

	// remap PA11 to PA9
	RCC->APBENR2 |= RCC_APBENR2_SYSCFGEN;
	SYSCFG->CFGR1 |= SYSCFG_CFGR1_UCPD2_STROBE | SYSCFG_CFGR1_UCPD1_STROBE;
	// SYSCFG->CFGR1 |= SYSCFG_CFGR1_PA11_RMP;

	// init gpio
	// enable clock for port a, b, and c
	RCC->IOPENR |= RCC_IOPENR_GPIOAEN | RCC_IOPENR_GPIOBEN | RCC_IOPENR_GPIOCEN;

	// PA6 (Trig_1) - output
	// PA7 (CAM_TRIG) - output
	// PA8 (ROT_A) - alternate
	// PA9 (ROT_B, fixed through pcb mod) - alternate
	// PA10 (TIA_SEL2) - output
	// PA11 (3V3_EN_MCU, fixed through pcb mod) - output
	// PA12 (TIA_SEL1) - output
	// PA13 (SWDIO) - alternate
	// PA14 (SWCLK) - alternate
	// PA15 (LCB_BLK) - output
	GPIOA->MODER = 0x695a5fff;
	GPIOA->AFR[1] |= 0x22;              // A8 and A9 -> AF2 (Timer 1 inputs)

	// port b output
	// PB4 (LCD_CS) -> output
	// PB5 (LCD_DC) -> output
	// PB6 (LCD_RES) -> output
	// PB7 (MOSI) -> alternate function
	// PB8 (MISO) -> alternate function
	GPIOB->MODER = 0xFFFE95FF;
	GPIOB->OSPEEDR = 0x0003EA00;        // all high speed for B4 to B6, very high speed for B7-B8
	GPIOB->AFR[0] |= 0x01 << 28;        // B7 -> AF1
	GPIOB->AFR[1] |= 0x01 << 0;         // B8 -> AF1

	// bring all output to reset state
	GPIOA->ODR = 0x800; // pa11
	GPIOB->ODR = 0;
	GPIOC->ODR = 0;

	// init spi, clk/2 (32Mhz), master
	RCC->APBENR1 |= RCC_APBENR1_SPI2EN;
	SPI2->CR2 = 0x0700;
	SPI2->CR1 = 0x0304;

	// init adc
	RCC->APBENR2 |= RCC_APBENR2_ADCEN;
	// Disable ADC in case it was beforehand
	if(ADC1->CR & ADC_CR_ADEN){
		ADC1->CR |= ADC_CR_ADDIS;
		while(ADC1->CR & ADC_CR_ADDIS);
	}

	// init timer 1 in quatrature input mode (pa8-ch1 and pa9-ch2)
	RCC->APBENR2 |= RCC_APBENR2_TIM1EN;
	TIM1->SMCR = 0x0003;        // encoder mode 3 (up and down)
	TIM1->CCER = 0x22;      // falling edge
	TIM1->CCMR1 = (0b0110 << TIM_CCMR1_IC1F_Pos) | (0b0110 << TIM_CCMR1_IC1F_Pos) | TIM_CCMR1_CC1S_1 | TIM_CCMR1_CC2S_1;
	TIM1->CR2 = 0x0;
	TIM1->CR1 = 0x1;

	// init timer 14 as the real time interrupt of 1mS
	RCC->APBENR2 |= RCC_APBENR2_TIM14EN;
	TIM14->PSC = 512;
	TIM14->ARR = 125;
	TIM14->CNT = 0;
	TIM14->CR1 = TIM_CR1_URS;
	TIM14->DIER = TIM_DIER_UIE;

	// set led backlight on
	GPIOA->ODR |= 1 << 15;
	DISP_CS_1;

	NVIC_EnableIRQ(TIM14_IRQn);

	SPI2->CR1 |= SPI_CR1_SPE;
}

int main(void)
{
	initMcu();

	gc9a01_init();
	gc9a01_fill_screen(gc9a01_color_black);


	// test draw of a circle with given angle
	float angle = 0.7853981633974483;		// 45 deg

	gc9a01_set_addr_window(0, 0, 240-1, 240-1);

	DISP_DC_0;
	DISP_CS_0;
	SPI_Write_Byte(0x2C);
	DISP_DC_1;
	for(int y=0;y<240;y++){
		for(int x=0;x<240;x++){
			if(atan2(x-120, 120-y) > angle){
				gc9a01_send_color(gc9a01_color_red);
			}
			else{
				gc9a01_send_color(gc9a01_color_black);
			}
		}
	}

	DISP_CS_1;

	for(EVER){
		
	}
	return 0;
}

/*
 * real time interrupt triggered by TIMx
 * this runs every 1Khz
 */
void TIM14_IRQHandler(void){
	TIM14->SR = 0;		// clear pending interrupt

}

void line_draw_vert_abstract(uint8_t *canvasBuff, uint16_t canvasW, uint16_t x, uint16_t h){
	int16_t canvasIdx;

	for(int y=0;y<h;y++){
		canvasIdx = (y*canvasW) + x / 8;
		canvasBuff[canvasIdx] |= 1 << (x & 0b111);
	}
}

/**
 * draws a canvas
 * buffer is [Y][X/8], and is bit filled
 */
void line_draw_abstract(uint8_t *canvasBuff, uint16_t canvasW, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1){
	// https://en.wikipedia.org/wiki/Bresenham's_line_algorithm#All_cases
	int16_t dx = abs(x1-x0);
	int16_t dy = -abs(y1-y0);
	int16_t sx = x0 < x1 ? 1 : -1;
	int16_t sy = y0 < y1 ? 1 : -1;
	int16_t error = dx + dy;

	int16_t canvasIdx;

    while(1){
        canvasIdx = (y0*canvasW) + x0 / 8;
        if(canvasIdx < 0){
            canvasIdx = 0;
        }
        canvasBuff[canvasIdx] |= 1 << (x0 & 0b111);

		if((2*error) >= dy){
			if(x0 == x1){break;}
			error += dy;
			x0 += sx;
		}
		if((2*error) <= dx){
			if(y0 == y1){break;}
			error += dx;
			y0 += sy;
		}

	}
}
