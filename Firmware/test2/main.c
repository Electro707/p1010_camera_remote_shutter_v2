#include "stm32g0xx.h"
#include "stm32g0xx_ll_rcc.h"
#include "stm32g0xx_ll_system.h"
#include "stm32g0xx_ll_utils.h"
#include "gc9a01.h"
#include "config.h"
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "nanoprintf.h"

void selectTiaSens(uint8_t sens);
void line_draw_abstract(uint8_t *canvasBuff, uint16_t canvasW, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void line_draw_vert_abstract(uint8_t *canvasBuff, uint16_t canvasW, uint16_t x, uint16_t h);

// #define BYTES_PER_PIXEL (LV_COLOR_FORMAT_GET_SIZE(LV_COLOR_FORMAT_RGB565))
// static uint8_t buf1[240 * 240 / 10 * BYTES_PER_PIXEL];

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

    ADC1->CFGR2 = 0x40000000;       // PCLK/2
    ADC1->SMPR = (0b11 << ADC_SMPR_SMP1_Pos);   // 12.5 ADC clock cycles for sampling time
    ADC1->CHSELR = 1 << 1;          // light as input
    // ADC1->CHSELR = 1 << 3;          // mic as input
    ADC1->CR = ADC_CR_ADVREGEN;      // ADVREGEN=1

    // init timer 1 in quatrature input mode (pa8-ch1 and pa9-ch2)
    RCC->APBENR2 |= RCC_APBENR2_TIM1EN;
    TIM1->SMCR = 0x0003;        // encoder mode 3 (up and down)
    TIM1->CCER = 0x22;      // falling edge
    TIM1->CCMR1 = (0b0110 << TIM_CCMR1_IC1F_Pos) | (0b0110 << TIM_CCMR1_IC1F_Pos) | TIM_CCMR1_CC1S_1 | TIM_CCMR1_CC2S_1;
    TIM1->CR2 = 0x0;
    TIM1->CR1 = 0x1;

    // set led backlight on
    GPIOA->ODR |= 1 << 15;
    DISP_CS_1;

    SPI2->CR1 |= 1 << 6;
}

void adcCal(void){
    unsigned int cal = 0;
    uint8_t n = 8;
    while(n--){
        ADC1->CR |= ADC_CR_ADCAL;
        while(ADC1->CR & ADC_CR_ADCAL);
        cal += ADC1->CALFACT + 1;
    }
    cal >>= 3;
    ADC1->CALFACT = cal;
}

int main(void)
{
    boundingBox_t box1;
    initMcu();

    // selectTiaSens(1);
    // selectTiaSens(2);
    selectTiaSens(3);


    gc9a01_init();
    // gc9a01_fill_screen(gc9a01_color_red);       LL_mDelay(100);
    // gc9a01_fill_screen(gc9a01_color_green);     LL_mDelay(100);
    // gc9a01_fill_screen(gc9a01_color_blue);      LL_mDelay(100);
    gc9a01_fill_screen(gc9a01_color_white);       LL_mDelay(100);
    gc9a01_fill_screen(gc9a01_color_black);     LL_mDelay(100);


    // adcCal();

    // gc9a01_print_text("Hello!", 100, 70, 0x00FFFF, 0, ALIGN_LEFT, NULL);
    // gc9a01_print_text("Hello!", 100, 110, 0x00FFFF, 0, ALIGN_CENTER, NULL);
    // gc9a01_print_text("Hello!", 100, 150, 0x00FFFF, 0, ALIGN_RIGHT, NULL);

    // gc9a01_print_text("123", box1.x1, box1.y0, 0xFFFF, 0, ALIGN_LEFT, NULL);

    // for(volatile unsigned int y=100;y<110;y++){
    //     gc9a01_point(y, y, 0xFFFFFF);
    // }
    // LL_mDelay(1000);

    // box1.x0 = 50; box1.x1 = 50+25; box1.y0 = 50; box1.y1 = 50+50;
    // gc9a01_draw_rect(&box1, gc9a01_color_red);
    // box1.x0 = 50; box1.y0 = 150; box1.x1 = 50+30; box1.y1 = 150+50;
    // gc9a01_draw_rect(&box1, gc9a01_color_blue);
    // box1.x0 = 150; box1.y0 = 150; box1.x1 = 150+30; box1.y1 = 150+50;
    // gc9a01_draw_rect(&box1, gc9a01_color_green);


    box1.x1 = 50; box1.y0 = 150;
    // gc9a01_print_text_med("Number: ", 20, 70, 0xFFFF, 0, ALIGN_LEFT, &box1);


    // for(int y=0;y<240;y++){
    //     gc9a01_line(0, 0, 239, y, gc9a01_color_white);
    // }
    // for(int x=239;x>=0;x--){
    //     gc9a01_line(0, 0, x, 239, gc9a01_color_green);
    // }

    // for(int y=0;y<240;y++){
    //     gc9a01_line(0, 0, 239, y, gc9a01_color_blue);
    // }
    // for(int x=239;x>=0;x--){
    //     gc9a01_line(0, 0, x, 239, gc9a01_color_red);
    // }
    

    // uint8_t n = 0;
    char nStr[5];

#define DATA_LEN 80
#define CANVAS_WIDTH    240
#define CANVAS_HEIGHT   100
#define CANVAS_WIDTH_BYTE (CANVAS_WIDTH/8)

    float data[DATA_LEN];
    uint8_t dataIdx = 0;
    uint8_t convData[DATA_LEN];
    memset(data, 0, DATA_LEN*sizeof(float));
    memset(convData, 0, DATA_LEN);

    TIM1->CNT = 100;

    uint8_t canvas[CANVAS_HEIGHT*CANVAS_WIDTH_BYTE];         // width = 240, 240/8=30 for rgb

    float currDat;
    float datMin, datMax, datSpan;

    for(int y=0;y<CANVAS_HEIGHT;y++){
        canvas[y*CANVAS_WIDTH_BYTE] = 0xFF;
    }
    gc9a01_draw_bit_canvas(canvas, 0, 50, CANVAS_WIDTH, CANVAS_HEIGHT, gc9a01_color_red);
    
    // for(EVER){
    // }

    ADC1->CR |= ADC_CR_ADEN;       // enable ADC

    ADC1->CR |= ADC_CR_ADSTART;
    
    for(EVER){

        currDat = 0.0;
        for(int i=0;i<5;i++){
            while((ADC1->ISR & ADC_ISR_EOC) == 0);
            currDat += (float)(ADC1->DR);
            // currDat = (float)(dataIdx);
            ADC1->CR |= ADC_CR_ADSTART;
        }
        currDat /= 5;

        data[dataIdx++] = currDat;
        if(dataIdx >= DATA_LEN){
            dataIdx = 0;
        }
        
        // // todo: inefficient, but easy to implement
        datMin = 1000000.0;
        datMax = 0;
        for(int i=0;i<DATA_LEN;i++){
            if(data[i] < datMin){
                datMin = data[i];
            }
            if(data[i] > datMax){
                datMax = data[i];
            }
        }
        datSpan = datMax - datMin;
        if(datSpan == 0.0){datSpan = 1.0;}
        for(int i=0;i<DATA_LEN;i++){
            convData[i] = (uint8_t)(((data[i]-datMin)/datSpan)*CANVAS_HEIGHT);
        }

        memset(canvas, 0, CANVAS_HEIGHT*CANVAS_WIDTH_BYTE);
        for(int i=0;i<DATA_LEN-1;i++){
            line_draw_abstract(canvas, CANVAS_WIDTH_BYTE, i*3, (CANVAS_HEIGHT-1)-convData[i], (i+1)*3, (CANVAS_HEIGHT-1)-convData[i+1]);
        }

        gc9a01_draw_bit_canvas(canvas, 0, 50, 240, CANVAS_HEIGHT, gc9a01_color_red);
        gc9a01_vert_line(dataIdx*3, 50, 150, gc9a01_color_green);


        // gc9a01_fill_screen(gc9a01_color_white);
        // gc9a01_draw_rect(50, 70, 100, 20, 0);
        // _gc9a01_send_single_cmd_data(0x36, 0x40);

        // npf_snprintf(nStr, 5, "%04u", TIM1->CNT);
        // gc9a01_print_text_med(nStr, box1.x1, box1.y0, 0xFFFF, 0, ALIGN_LEFT, NULL);
        // LL_mDelay(50);

        // LL_mDelay(25);
        // n++;
        // gc9a01_fill_screen(gc9a01_color_white);
        // gc9a01_draw_rect(50, 50, 50, 20, 0);
        // _gc9a01_send_single_cmd_data(0x36, 0);

    }
    return 0;
}

void selectTiaSens(uint8_t sens){
    switch(sens){
        case 0:
            GPIOA->BRR = (1 << PIN_TIA_SEL_1);          // clear
            GPIOA->BRR = (1 << PIN_TIA_SEL_2);          // clear
            break;
        case 1:
            GPIOA->BSRR = (1 << PIN_TIA_SEL_1);         // set
            GPIOA->BSRR = (1 << PIN_TIA_SEL_2*2);       // clear
            break;
        case 2:
            GPIOA->BSRR = (1 << PIN_TIA_SEL_1*2);       // clear
            GPIOA->BSRR = (1 << PIN_TIA_SEL_2);         // set
            break;
        case 3:
            GPIOA->BSRR = (1 << PIN_TIA_SEL_1);         // set
            GPIOA->BSRR = (1 << PIN_TIA_SEL_2);         // set
            break;
        default:
            // todo: error handling
            break;
    }
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
