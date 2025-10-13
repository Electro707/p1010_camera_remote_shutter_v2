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

#include "objCamera.h"

#define CANVAS_WIDTH    240
#define CANVAS_HEIGHT   240
#define CANVAS_WIDTH_BYTE (CANVAS_WIDTH/8)

#define FACE_VERTEX_SIZE	3

typedef struct{
	u32 rows;
	u32 cols;
} matrixSize_s;

void line_draw_abstract(uint8_t *canvasBuff, uint16_t canvasW, int x0, int y0, int x1, int y1);
void line_draw_vert_abstract(uint8_t *canvasBuff, uint16_t canvasW, uint16_t x, uint16_t h);

void matrixMult(float *srcA, float *srcB, float *dst, matrixSize_s sizeA, matrixSize_s sizeB);

float toViewCoordsMatrixScale[4*4] = {
	40, 0, 0, 0,
	0, 40, 0, 0,
	0, 0, 40, 0,
	0, 0, 0, 1,
}; 

float toViewCoordsMatrixTranslate[4*4] = {
	1, 0, 0, 0,
	0, 1, 0, 0,
	0, 0, 1, 0,
	120, 120, 0, 1,
}; 

uint8_t canvas[CANVAS_HEIGHT*CANVAS_WIDTH_BYTE];         // width = 240, 240/8=30 for rgb


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

int getPlaneZ(int x, int y, int *planeNormal, int *planeVertex){
	if(planeNormal[2] == 0){
		return -1000;
	}

	int d = (planeNormal[0]*planeVertex[0]) + (planeNormal[1]*planeVertex[1]) + (planeNormal[2]*planeVertex[2]);
	int z = (x*planeNormal[0])+(y*planeNormal[1]) - d;
	z /= -planeNormal[2];
	return (int)z;
}

#define DRAW_FACE_TEST

void drawObj(int *obj, uint *edgeToVertex, uint edgeToVertexLen, uint *faceDef, uint faceDefLen, int *faceNormals, uint *faceToNormalIdx){
	uint vertex1, vertex2;
	int *pos;
	int *posNext;

#ifndef DRAW_FACE_TEST
	for(uint edgeI=0;edgeI<edgeToVertexLen;edgeI++){
		vertex1 = edgeToVertex[edgeI*2];
		vertex2 = edgeToVertex[(edgeI*2)+1];

		pos = &obj[4*vertex1];
		posNext = &obj[4*vertex2];
		line_draw_abstract(canvas, CANVAS_WIDTH_BYTE,
				pos[0],
				pos[1],
				posNext[0],
				posNext[1]);
	}
#else
	
	u32 *face;
	int *normalPlane;
	int16_t canvasIdx;

	int edgeFunc[FACE_VERTEX_SIZE*faceDefLen];
	int edgeDx[FACE_VERTEX_SIZE*faceDefLen];
	int edgeDy[FACE_VERTEX_SIZE*faceDefLen];

	// line function
	bool toDraw;
	
	for(uint edgeI=0;edgeI<edgeToVertexLen;edgeI++){
		vertex1 = edgeToVertex[edgeI*2];
		vertex2 = edgeToVertex[(edgeI*2)+1];

		pos = &obj[4*vertex1];
		posNext = &obj[4*vertex2];

		// reject edge if two faces connected to it have same normal
		// note: deleted out code as this was done by the Python script already

		int x0 = pos[0];
		int y0 = pos[1];
		int z0 = pos[2];
		int x1 = posNext[0];
		int y1 = posNext[1];
		int z1 = posNext[2];
		
		int dx = abs(x1-x0);
		int dy = abs(y1-y0);
		int dz = abs(z1-z0);
		int sx = x0 < x1 ? 1 : -1;
		int sy = y0 < y1 ? 1 : -1;
		int sz = z0 < z1 ? 1 : -1;

		int dm = dx;
		if(dy > dm)
			dm = dy;
		if(dz > dm)
			dm = dz;
		
		int dmI = dm;

		// https://zingl.github.io/Bresenham.pdf
		
		for(uint fI=0;fI<faceDefLen;fI++){
			face = &faceDef[fI*FACE_VERTEX_SIZE];
			for(int i=0;i<FACE_VERTEX_SIZE;i++){
				pos = &obj[(4*face[(i)%FACE_VERTEX_SIZE])];
				posNext = &obj[(4*face[(i+1)%FACE_VERTEX_SIZE])];

				int eDx = posNext[0] - pos[0];
				int eDy = posNext[1] - pos[1];

				edgeFunc[fI*FACE_VERTEX_SIZE + i] = ((x0-pos[0])*eDy) - ((y0-pos[1])*eDx);
				edgeDx[fI*FACE_VERTEX_SIZE + i] = eDx * sy;
				edgeDy[fI*FACE_VERTEX_SIZE + i] = eDy * sx;
			}
		}
		
		
		for(x1=y1=z1= dmI/2; dmI-- >= 0; ){
			toDraw = true;
			
			for(uint fI=0;fI<faceDefLen;fI++){
				face = &faceDef[fI*FACE_VERTEX_SIZE];
				normalPlane = &faceNormals[faceToNormalIdx[fI]*4];

				// skip over faces which are on the exact edge, which cannot by definition be above the line
				uint isVertexInFace = 0;
				uint isInsideCnt = 0;
				for(int i=0;i<FACE_VERTEX_SIZE;i++){
					if(face[i] == vertex1 || face[i] == vertex2){
						isVertexInFace += 1;
						if(isVertexInFace >= 2){
							break;
						}
					}
					if(edgeFunc[fI*FACE_VERTEX_SIZE + i] >= 0){
						isInsideCnt += 1;
						if(isVertexInFace == FACE_VERTEX_SIZE){
							break;
						}
					}
				}

				if(isVertexInFace >= 2){
					continue;
				}
				if(isInsideCnt == FACE_VERTEX_SIZE){
					int faceZ = getPlaneZ(x0, y0, normalPlane, &obj[4*face[0]]);
					if((z0 - faceZ) < -2){
						toDraw = false;
						break;
					}
				}
			}
			

			if(toDraw){
				canvasIdx = (y0*CANVAS_WIDTH_BYTE) + (x0 >> 3);
				if(canvasIdx < 0){
					canvasIdx = 0;
				}
				canvas[canvasIdx] |= 1 << (x0 & 0b111);
			}

			x1 -= dx; 
			if(x1 < 0){
				x1 += dm;
				x0 += sx;
				for(uint i=0;i<FACE_VERTEX_SIZE*faceDefLen;i++){
					edgeFunc[i] += edgeDy[i];
				}
			}

			y1 -= dy;
			if(y1 < 0){
				y1 += dm;
				y0 += sy;
				for(uint i=0;i<FACE_VERTEX_SIZE*faceDefLen;i++){
					edgeFunc[i] -= edgeDx[i];
				}
			}

			z1 -= dz;
			if(z1 < 0){
				z1 += dm;
				z0 += sz;
			}
		}

		
	}
#endif
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

	float newPos[VERTEX_SIZE*4];
	float newPosNormal[NORMAL_SIZE*4];
	float txMatrix[4*4];
	float txMatrix2[4*4];
	float rot = 0.0;

	int newPosInt[VERTEX_SIZE*4];
	int newPosNormalInt[NORMAL_SIZE*4];

	for(EVER){
		memset(canvas, 0, CANVAS_HEIGHT*CANVAS_WIDTH_BYTE);

		createTransformMatrix(-20., rot, rot*0.5, txMatrix);
		matrixMult(toViewCoordsMatrixScale, txMatrix, txMatrix2, (matrixSize_s){4, 4}, (matrixSize_s){4, 4});
		matrixMult(txMatrix2, toViewCoordsMatrixTranslate, txMatrix, (matrixSize_s){4, 4}, (matrixSize_s){4, 4});


		// matrixMult(objectVertex, txMatrix, newPos, (matrixSize_s){32, 4}, (matrixSize_s){4, 4});
		matrixMult(objectNormal, txMatrix2, newPosNormal, (matrixSize_s){NORMAL_SIZE, 4}, (matrixSize_s){4, 4});
		matrixMult(objectVertex, txMatrix, newPos, (matrixSize_s){VERTEX_SIZE, 4}, (matrixSize_s){4, 4});

		for(int i=0;i<VERTEX_SIZE*4;i++){
			newPosInt[i] = (int)newPos[i];
		}
		for(int i=0;i<NORMAL_SIZE*4;i++){
			newPosNormalInt[i] = (int)newPosNormal[i];
		}

		drawObj(newPosInt, objectEdge, EDGE_SIZE, objectFace, FACE_SIZE, newPosNormalInt, objectNormalFaceIdx);

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
void line_draw_abstract(uint8_t *canvasBuff, uint16_t canvasW, int x0, int y0, int x1, int y1){
	// https://en.wikipedia.org/wiki/Bresenham's_line_algorithm#All_cases
	int dx = abs(x1-x0);
	int dy = -abs(y1-y0);
	int sx = x0 < x1 ? 1 : -1;
	int sy = y0 < y1 ? 1 : -1;
	int error = dx + dy;

	int canvasIdx;

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