#include "stm32g0xx.h"
#include "stm32g0xx_ll_rcc.h"
#include "stm32g0xx_ll_system.h"
#include "stm32g0xx_ll_utils.h"
#include "gc9a01.h"
#include "config.h"
#include <stddef.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#define CANVAS_WIDTH    240
#define CANVAS_HEIGHT   240
#define CANVAS_WIDTH_BYTE (CANVAS_WIDTH/8)

typedef struct{
	u32 rows;
	u32 cols;
} matrixSize_s;

void line_draw_abstract(uint8_t *canvasBuff, uint16_t canvasW, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void line_draw_vert_abstract(uint8_t *canvasBuff, uint16_t canvasW, uint16_t x, uint16_t h);


void matrixMult(float *srcA, float *srcB, float *dst, matrixSize_s sizeA, matrixSize_s sizeB);

uint8_t canvas[CANVAS_HEIGHT*CANVAS_WIDTH_BYTE];         // width = 240, 240/8=30 for rgb

/* defines for object */
float objectVertex[32*4] = {
	2.000000, 1.000000, -0.750000, 1.000000,
	2.000000, -1.000000, -0.750000, 1.000000,
	2.000000, 1.000000, 0.750000, 1.000000,
	2.000000, -1.000000, 0.750000, 1.000000,
	-2.000000, 1.000000, -0.750000, 1.000000,
	-2.000000, -1.000000, -0.750000, 1.000000,
	-2.000000, 1.000000, 0.750000, 1.000000,
	-2.000000, -1.000000, 0.750000, 1.000000,
	0.000000, 0.750000, -1.500000, 1.000000,
	0.000000, 0.750000, -0.750000, 1.000000,
	0.530330, 0.530330, -1.500000, 1.000000,
	0.530330, 0.530330, -0.750000, 1.000000,
	0.750000, 0.000000, -1.500000, 1.000000,
	0.750000, -0.000000, -0.750000, 1.000000,
	0.530330, -0.530330, -1.500000, 1.000000,
	0.530330, -0.530330, -0.750000, 1.000000,
	0.000000, -0.750000, -1.500000, 1.000000,
	0.000000, -0.750000, -0.750000, 1.000000,
	-0.530330, -0.530330, -1.500000, 1.000000,
	-0.530330, -0.530330, -0.750000, 1.000000,
	-0.750000, 0.000000, -1.500000, 1.000000,
	-0.750000, -0.000000, -0.750000, 1.000000,
	-0.530330, 0.530330, -1.500000, 1.000000,
	-0.530330, 0.530330, -0.750000, 1.000000,
	-1.600000, 1.000000, 0.300000, 1.000000,
	-1.600000, 1.300000, 0.300000, 1.000000,
	-1.600000, 1.000000, -0.300000, 1.000000,
	-1.600000, 1.300000, -0.300000, 1.000000,
	-1.000000, 1.000000, 0.300000, 1.000000,
	-1.000000, 1.300000, 0.300000, 1.000000,
	-1.000000, 1.000000, -0.300000, 1.000000,
	-1.000000, 1.300000, -0.300000, 1.000000,
};
uint objectEdge[48*2] =     {
	0, 4,	4, 6,	6, 2,	2, 0,
	3, 2,	6, 7,	7, 3,	4, 5,
	5, 7,	5, 1,	1, 3,	1, 0,
	8, 9,	9, 11,	11, 10,	10, 8,
	11, 13,	13, 12,	12, 10,	13, 15,
	15, 14,	14, 12,	15, 17,	17, 16,
	16, 14,	17, 19,	19, 18,	18, 16,
	19, 21,	21, 20,	20, 18,	9, 23,
	23, 21,	23, 22,	22, 20,	8, 22,
	24, 25,	25, 27,	27, 26,	26, 24,
	27, 31,	31, 30,	30, 26,	31, 29,
	29, 28,	28, 30,	29, 25,	24, 28,
};


uint16_t scaleVectorToDraw(float pos){
	return (uint16_t)((pos * 40) + 120);
}

void createTransformMatrix(float rotX, float rotY, float rotZ, float *resultMatrix){
	float tmpA[4*4];
	float tmpB[4*4];

	rotX = rotX * (M_PI/180);
	rotY = rotY * (M_PI/180);
	rotZ = rotZ * (M_PI/180);

	float rotationMatrixX[4*4] = {
		1, 0,         0,          0,
		0, cos(rotX), -sin(rotX), 0,
		0, sin(rotX), cos(rotX),  0,
		0, 0,         0,          1,
	};
	float rotationMatrixY[4*4] = {
		cos(rotY),  0, sin(rotY), 0,
		0,          1, 0,         0,
		-sin(rotY), 0, cos(rotY), 0,
		0,          0, 0,         1,
	};
	float rotationMatrixZ[4*4] = {
		cos(rotZ), -sin(rotZ), 0, 0,
		sin(rotZ), cos(rotZ),  0, 0,
		0,         0,          1, 0,
		0,         0,          0, 1,
	};

	float inverseMatrix[4*4] = {
		1, 0, 0, 0,
		0, -1,  0, 0,
		0,     0,          1, 0,
		0,         0,          0, 1,
	};

	matrixMult(rotationMatrixX, rotationMatrixY, tmpA, (matrixSize_s){4, 4}, (matrixSize_s){4, 4});
	matrixMult(tmpA, rotationMatrixY, tmpB, (matrixSize_s){4, 4}, (matrixSize_s){4, 4});
	matrixMult(tmpB, rotationMatrixZ, tmpA, (matrixSize_s){4, 4}, (matrixSize_s){4, 4});
	matrixMult(tmpA, inverseMatrix, resultMatrix, (matrixSize_s){4, 4}, (matrixSize_s){4, 4});
}

void matrixMult(float *srcA, float *srcB, float *dst, matrixSize_s sizeA, matrixSize_s sizeB){
	float sum;

	// assert(sizeA.cols == sizeB.rows);

	u32 m = sizeA.rows;
	u32 p = sizeB.cols;
	u32 n = sizeA.cols;

	// memset(dst, 0, m * p * sizeof(float));

	for(int i = 0; i < m; i++){
		for(int j = 0; j < p; j++){
			sum = 0;
			for(int k = 0; k < n; k++){
				sum += srcA[k+(i*sizeA.cols)] * srcB[j+(k*sizeB.cols)];
			}
			dst[(i*sizeB.cols) + j] = sum;
		}
	}
}

void drawObj(float *obj, uint *faceDef, uint objEdgeLen){
	float *pos;
	float *posNext;

	for(int i=0;i<objEdgeLen;i++){
		pos = &obj[4*(faceDef[i*2])];
		posNext = &obj[4*(faceDef[(i*2)+1])];

		line_draw_abstract(canvas, CANVAS_WIDTH_BYTE,
				scaleVectorToDraw(pos[0]),
				scaleVectorToDraw(pos[1]),
				scaleVectorToDraw(posNext[0]),
				scaleVectorToDraw(posNext[1]));
	}

	// for(int i=0;i<objEdgeLen;i++){
	// 	putPixel(scaleVectorToDraw(obj[4*i]), scaleVectorToDraw(obj[(4*i)+1]), 0x00FFFFFF);
	// }
	
}

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

	// init spi, clk/2 (32Mhz, max possible), master
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
	gc9a01_fill_screen(gc9a01_color_red);

	// gc9a01_set_addr_window(0, 0, 240-1, 240-1);

	float newPos[32*4];
	float txMatrix[4*4];
	float rot = 0.0;

	for(EVER){
		memset(canvas, 0, CANVAS_HEIGHT*CANVAS_WIDTH_BYTE);

		createTransformMatrix(-20., rot, rot*0.5, txMatrix);
		matrixMult(objectVertex, txMatrix, newPos, (matrixSize_s){32, 4}, (matrixSize_s){4, 4});
		drawObj(newPos, objectEdge, 48);

		// line_draw_vert_abstract(canvas, CANVAS_WIDTH_BYTE, 50, 100);

		gc9a01_draw_bit_canvas(canvas, 0, 0, CANVAS_WIDTH, CANVAS_HEIGHT, gc9a01_color_white);

		rot += 1;
		if(rot > 360){
			rot = 0;
		}

		// LL_mDelay(100);
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
		canvasIdx = (y*canvasW) + (x >> 3);
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
        canvasIdx = (y0*canvasW) + (x0 >> 3);
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