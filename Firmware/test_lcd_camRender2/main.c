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

typedef int16_t q15;

void line_draw_abstract(uint8_t *canvasBuff, uint16_t canvasW, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void line_draw_vert_abstract(uint8_t *canvasBuff, uint16_t canvasW, uint16_t x, uint16_t h);


void matrixMult(q15 *srcA, q15 *srcB, q15 *dst, matrixSize_s sizeA, matrixSize_s sizeB);

uint8_t canvas[CANVAS_HEIGHT*CANVAS_WIDTH_BYTE];         // width = 240, 240/8=30 for rgb

/* defines for object */
q15 objectVertex[32*4] = {
        31130, 15565, -11674, 31130,
        31130, -15565, -11674, 31130,
        31130, 15565, 11674, 31130,
        31130, -15565, 11674, 31130,
        -31130, 15565, -11674, 31130,
        -31130, -15565, -11674, 31130,
        -31130, 15565, 11674, 31130,
        -31130, -15565, 11674, 31130,
        0, 11674, -23347, 31130,
        0, 11674, -11674, 31130,
        8254, 8254, -23347, 31130,
        8254, 8254, -11674, 31130,
        11674, 0, -23347, 31130,
        11674, 0, -11674, 31130,
        8254, -8254, -23347, 31130,
        8254, -8254, -11674, 31130,
        0, -11674, -23347, 31130,
        0, -11674, -11674, 31130,
        -8254, -8254, -23347, 31130,
        -8254, -8254, -11674, 31130,
        -11674, 0, -23347, 31130,
        -11674, 0, -11674, 31130,
        -8254, 8254, -23347, 31130,
        -8254, 8254, -11674, 31130,
        -24904, 15565, 4669, 31130,
        -24904, 20234, 4669, 31130,
        -24904, 15565, -4669, 31130,
        -24904, 20234, -4669, 31130,
        -15565, 15565, 4669, 31130,
        -15565, 20234, 4669, 31130,
        -15565, 15565, -4669, 31130,
        -15565, 20234, -4669, 31130,
};
uint8_t objectEdge[48*2] =     {
        0, 4,   4, 6,   6, 2,   2, 0,
        3, 2,   6, 7,   7, 3,   4, 5,
        5, 7,   5, 1,   1, 3,   1, 0,
        8, 9,   9, 11,  11, 10, 10, 8,
        11, 13, 13, 12, 12, 10, 13, 15,
        15, 14, 14, 12, 15, 17, 17, 16,
        16, 14, 17, 19, 19, 18, 18, 16,
        19, 21, 21, 20, 20, 18, 9, 23,
        23, 21, 23, 22, 22, 20, 8, 22,
        24, 25, 25, 27, 27, 26, 26, 24,
        27, 31, 31, 30, 30, 26, 31, 29,
        29, 28, 28, 30, 29, 25, 24, 28,
};

q15 sat16(int32_t x){
	if (x > 0x7FFF) return 0x7FFF;
	else if (x < -0x8000) return -0x8000;
	else return (q15)x;
}

q15 q15Mult(q15 a, q15 b){
	int32_t res = (int32_t)a * (int32_t)b;
	res += 16384;
	return sat16(res >> 15);
}

uint16_t scaleVectorToDraw(q15 pos){
	return (uint16_t)(q15Mult(pos, 100.) + 120);
}


// todo: this is converting with floats for now, todo lookup table
// rad is from 0 to 1.0 in q15 format, which is converted to 0 to 2*pi
q15 cosFP(q15 rad){
	float tmp;
	
	tmp = rad;
	tmp /= 32768.0;
	tmp *= M_PI * 2;

	tmp = cos(tmp);

	tmp *= 32768.0;
	return sat16(tmp);
}
q15 sinFP(q15 rad){
	float tmp;
	
	tmp = rad;
	tmp /= 32767.0;
	tmp *= M_PI * 2;

	tmp = sin(tmp);

	tmp *= 32767.0;
	return (q15)tmp;
}

void createTransformMatrix(q15 rotX, q15 rotY, q15 rotZ, q15 *resultMatrix){
	q15 tmpA[4*4];
	q15 tmpB[4*4];

	q15 rotationMatrixX[4*4] = {
		32767, 0,         0,          0,
		0, cosFP(rotX), -sinFP(rotX), 0,
		0, sinFP(rotX), cosFP(rotX),  0,
		0, 0,         0,          32767,
	};
	q15 rotationMatrixY[4*4] = {
		cosFP(rotY),  0, sinFP(rotY), 0,
		0,          32767, 0,         0,
		-sinFP(rotY), 0, cosFP(rotY), 0,
		0,          0, 0,         32767,
	};
	q15 rotationMatrixZ[4*4] = {
		cosFP(rotZ), -sinFP(rotZ), 0, 0,
		sinFP(rotZ), cosFP(rotZ),  0, 0,
		0,         0,          32767, 0,
		0,         0,          0, 32767,
	};

	q15 inverseMatrix[4*4] = {
		32767, 0, 0, 0,
		0, -32768,  0, 0,
		0,     0,          32767, 0,
		0,         0,          0, 32767,
	};

	matrixMult(rotationMatrixX, rotationMatrixY, tmpA, (matrixSize_s){4, 4}, (matrixSize_s){4, 4});
	matrixMult(tmpA, rotationMatrixY, tmpB, (matrixSize_s){4, 4}, (matrixSize_s){4, 4});
	matrixMult(tmpB, rotationMatrixZ, tmpA, (matrixSize_s){4, 4}, (matrixSize_s){4, 4});
	matrixMult(tmpA, inverseMatrix, resultMatrix, (matrixSize_s){4, 4}, (matrixSize_s){4, 4});
}

void matrixMult(q15 *srcA, q15 *srcB, q15 *dst, matrixSize_s sizeA, matrixSize_s sizeB){
	int32_t sum;

	// assert(sizeA.cols == sizeB.rows);

	u32 m = sizeA.rows;
	u32 p = sizeB.cols;
	u32 n = sizeA.cols;

	memset(dst, 0, m * p * sizeof(q15));

	for(int i = 0; i < m; i++){
		for(int j = 0; j < p; j++){
			sum = 0;
			for(int k = 0; k < n; k++){
				sum += q15Mult(srcA[k+(i*sizeA.cols)], srcB[j+(k*sizeB.cols)]);
			}
			dst[(i*sizeB.cols) + j] = sat16(sum);
		}
	}
}

void drawObj(q15 *obj, uint8_t *faceDef, uint objEdgeLen){
	q15 *pos;
	q15 *posNext;

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

	q15 newPos[32*4];
	q15 txMatrix[4*4];
	q15 rot = 0;

	for(EVER){
		memset(canvas, 0, CANVAS_HEIGHT*CANVAS_WIDTH_BYTE);

		createTransformMatrix(-1820, rot, 0, txMatrix);
		matrixMult(objectVertex, txMatrix, newPos, (matrixSize_s){32, 4}, (matrixSize_s){4, 4});
		drawObj(newPos, objectEdge, 48);

		gc9a01_draw_bit_canvas(canvas, 0, 0, CANVAS_WIDTH, CANVAS_HEIGHT, gc9a01_color_white);

		rot += 91;
		if(rot > 0x7FFF){
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