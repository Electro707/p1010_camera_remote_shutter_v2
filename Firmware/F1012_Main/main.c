/**
 * Main F1012 Firmware
 *
 * This runs on an STM32G071K8Tx
 *
 * TODO:
 *		- have all values, instead of floats, be in mS increments (or more), and have a custom print function
 */
#include "stm32g0xx.h"
#include "stm32g0xx_ll_rcc.h"
#include "stm32g0xx_ll_system.h"
#include "stm32g0xx_ll_utils.h"
#include "gc9a01.h"
#include "config.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "nanoprintf.h"
#include "graphics.h"
#include "font.h"
#include "screensaver.h"

// run-time modes
typedef enum{
	STATE_RESET,		// reset state
	STATE_STANDBY,		// standby, not doing anything with pictures
	STATE_EDITING_VAR,	// editing variable in UI
	STATE_ARMED,		// armed, waiting to take picture
	STATE_TRIG,			// camera triggered
	STATE_TIMELAPSE,	// doing timelapse
	STATE_SCREENSAVER,	// screen saver state
	STATE_SHUTDOWN		// shutdown
}stateMachine_e;

// to keep track of what we are selecting through UI
typedef enum{
	VAR_TRIG_TIME = 0,
	VAR_SHUTTER_TIME,
	VAR_TIMELAPSE_N,
	VAR_TIMELAPSE_DUR,
	VAR_END,
}configVars_e;

typedef enum{
	HOME_ELE_SHUTTER_TIME = 0,
	HOME_ELE_TRIG_TIME,
	HOME_ELE_TIMELAPSE_N,
	HOME_ELE_TIMELAPSE_DUR,
	HOME_ELE_START_BT,
	HOME_ELE_END,
}homeElements_e;

// trigger configs
struct config_s{
	float shutterDelay;			// sec, the time from trigger to shutter open
	float shutterSpeed;			// sec, the shutter speed
	int timelapseNPics;		    // n, number of pictures to take in trigger time. Set to -1 for infinite
	float timelapseInterval;	// sec, timelapse interval between different pictures 
};

struct button_s{
	// "internal" variables
	int validCnt;		// validation count for any press
	int lastState;		// the last button real state
	int preValidState;	// the button press level before validation count is started
	int pressedDur;		// ticks, how long the button was pressed for
	// signal for whover is using this button
	int pressedTrig;    // signal for when button has been pressed
	int depressedTrig;	// signal for when depressed
};

void buttonPressHandler(struct button_s *bt, uint buttonState);
void autoShutdownService(void);
void shutdownDevice(void);
void selectTiaSens(uint8_t sens);
void line_draw_abstract(uint8_t *canvasBuff, uint16_t canvasW, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void line_draw_vert_abstract(uint8_t *canvasBuff, uint16_t canvasW, uint16_t x, uint16_t h);

stateMachine_e state;
uint autoShutdownTimer;		// counter for auto shutting down

bool trigUpdateLcd;			// whether main should update something on the display

homeElements_e selectedElem;		// selected element on screen, can be button or editable variable
// homeElements_e lastSelectedElem;	// last value of selectedElem, used to draw only as needed
// uint editVarCurr;			// current variable that is edited. Only valid in state `STATE_EDITING_VAR`
uint lastEncoderState;
int encoderCnt = 0;			// local counter for the encoder
bool holdEncoderWaitForDepress = false;			// flag if we are waiting for the encoder to depress to not count previous hold state against counter

struct button_s encoderBt;
struct button_s leftBt;
struct button_s rightBt;

struct config_s conf;

float currentTrigTime;		// ticks, current state timer for trigger and arm

void initMcu(void){
	// init clock, 64Mhz system and periferal clock
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

	// PA4 (BT1) - input
	// PA5 (BT2)) - inpuit
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
	GPIOA->MODER = 0x695a50ff;
	GPIOA->AFR[1] |= 0x22;              // A8 and A9 -> AF2 (Timer 1 inputs)

	// port b output
	// PB1 (ROT_S) -> input
	// PB4 (LCD_CS) -> output
	// PB5 (LCD_DC) -> output
	// PB6 (LCD_RES) -> output
	// PB7 (MOSI) -> alternate function
	// PB8 (MISO) -> alternate function
	GPIOB->MODER = 0xFFFE95F3;
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

	// init timer 14 as the real time interrupt of 1mS	( 1/64e6 * 512 * 125 = 0.001)
	RCC->APBENR2 |= RCC_APBENR2_TIM14EN;
	TIM14->PSC = 512;
	TIM14->ARR = 125;
	TIM14->CNT = 0;
	TIM14->CR1 = TIM_CR1_URS;
	TIM14->DIER = TIM_DIER_UIE;

	// init timer 6 as the system timeout time, period of 10 seconds ( 1/32e6 * 40000 * 16000 = 10.0)
	RCC->APBENR1 |= RCC_APBENR1_TIM6EN;
	TIM6->CR1 = TIM_CR1_URS;
	TIM6->DIER = TIM_DIER_UIE;
	TIM6->PSC = 40000;
	TIM6->ARR = 16000;
	TIM6->CNT = 0;

	// set led backlight on
	GPIOA->ODR |= 1 << 15;
	DISP_CS_1;

	NVIC_EnableIRQ(TIM6_DAC_LPTIM1_IRQn);
	NVIC_EnableIRQ(TIM14_IRQn);

	SPI2->CR1 |= SPI_CR1_SPE;
}

void triggerCamera(bool trig){
	volatile uint32_t *reg;
	if(trig){
		reg = &GPIOB->BSRR;
	} else {
		reg = &GPIOB->BRR;
	}

	*reg = (1 << 7) | (1 << 2);
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

const char *varTexts[VAR_END] = {"Shutter delay:", "Shutter Time:", "Timelapse #Pics:", "Timelapse Delay:"};

void drawHomeScreenValuesSingle(int index, char *toWrite){
	uint y = 50 + 32*index;
	uint32_t fg = gc9a01_color_white;
	uint32_t bg = gc9a01_color_black;

	// select if highlighted based off if selected
	if(state == STATE_EDITING_VAR && selectedElem == index){
		fg = gc9a01_color_black;
		bg = gc9a01_color_white;
	}
	else {
		fg = gc9a01_color_white;
		bg = gc9a01_color_black;
	}

	gc9a01_print_text_sma(toWrite, 205, y, fg, bg, ALIGN_RIGHT, NULL);
}
void drawHomeScreenValuesAll(void){
	char nStr[10];
	int varI;
	float varF;
	uint y = 50;
	uint32_t fg = gc9a01_color_white;
	uint32_t bg = gc9a01_color_black;

	for(int i=0;i<VAR_END;i++){
		if(state == STATE_EDITING_VAR && selectedElem == i){
			fg = gc9a01_color_black;
			bg = gc9a01_color_white;
		}
		else {
			fg = gc9a01_color_white;
			bg = gc9a01_color_black;
		}

		switch(i){
			case VAR_SHUTTER_TIME:
				varF = conf.shutterSpeed;
				npf_snprintf(nStr, 10, "%5.2f", varF);
				break;
			case VAR_TRIG_TIME:
				varF = conf.shutterDelay;
				npf_snprintf(nStr, 10, "%5.2f", varF);
				break;
			case VAR_TIMELAPSE_N:
				varI = conf.timelapseNPics;
				npf_snprintf(nStr, 10, "%5d", varI);
				break;
			case VAR_TIMELAPSE_DUR:
				varF = conf.timelapseInterval;
				npf_snprintf(nStr, 10, "%5.2f", varF);
				break;
			default:
				break;
		}

		gc9a01_print_text_sma(nStr, 205, y, fg, bg, ALIGN_RIGHT, NULL);
		y += 32;
	}
}

void drawHomeScreenElementsStartBtProg(float prog){
	boundingBox_t textBox;
	uint8_t buttonCanvas[288];			// (64*36) / 8

	memset(buttonCanvas, 0, sizeof(buttonCanvas));

	const uint canvasW = 64;		// keep this power of two for convinience
	const uint canvasH = 36;		// keep this power of two for convinience
	const uint borderW = 2;

	uint32_t fg;
	
	// start button, background
	textBox.x0 = 0;			// width = 64
	textBox.x1 = canvasW;
	textBox.y0 = 0;			// height = 36
	textBox.y1 = 36;

	canvas_draw_rect(buttonCanvas, canvasW, &textBox, borderW);

	switch(state){
		case STATE_STANDBY:
		case STATE_EDITING_VAR:
			if(selectedElem == HOME_ELE_START_BT){
				fg = 0x57eb;
			} else {
				fg = 0xed23;
			}
			canvas_print_text(buttonCanvas, canvasW, "START", 32, 10, ALIGN_CENTER, 8, 16, spleenFont16);
			break;
		case STATE_TRIG:
		case STATE_ARMED:
		case STATE_TIMELAPSE:
			fg = 0xfb28;
			canvas_print_text(buttonCanvas, canvasW, "STOP", 32, 10, ALIGN_CENTER, 8, 16, spleenFont16);
			break;
		default:
			fg = 0xFFFF;
			break;
	}

	if(prog){
		if(prog > 1.0){
			prog = 1.0;
		}
		uint w = (uint)((canvasW-2*borderW) * prog);
		canvasFillWithInvert(buttonCanvas, canvasW, borderW, borderW, w, canvasH-2*borderW);
	}

	gc9a01_draw_bit_canvas(buttonCanvas, 88, 190, canvasW, canvasH, fg);
}

void drawHomeScreenElementsStartBt(void){
	drawHomeScreenElementsStartBtProg(0);
}

// updates a certain progress bar. prog is from 0 to 1
void drawHomeProgressUpdate(configVars_e config, float prog){
	boundingBox_t textBox;
	uint y = 50+16+(config*32);

	textBox.x0 = 50;
	textBox.x1 = 240-50;
	textBox.y0 = y+4;
	textBox.y1 = y+12;
	gc9a01_draw_rect(&textBox, gc9a01_color_white, 1);

	textBox.x1 = (uint16_t)(prog * 140.) + 50;
	gc9a01_draw_fill_rect_textBox(&textBox, gc9a01_color_white);
}

void drawHomeScreenElements(void){
	boundingBox_t textBox;
	uint y = 50;
	// gc9a01_fill_screen(gc9a01_color_black);

	uint32_t fg = gc9a01_color_white;
	uint32_t bg = gc9a01_color_black;

	for(int i=0;i<VAR_END;i++){
		if(state == STATE_STANDBY && selectedElem == i){
			fg = gc9a01_color_black;
			bg = gc9a01_color_white;
		}
		else {
			fg = gc9a01_color_white;
			bg = gc9a01_color_black;
		}

		gc9a01_print_text_sma(varTexts[i], 35, y, fg, bg, ALIGN_LEFT, NULL);
		y += 16;

		textBox.x0 = 50;
		textBox.x1 = 240-50;
		textBox.y0 = y+4;
		textBox.y1 = y+12;
		gc9a01_draw_fill_rect_textBox(&textBox, gc9a01_color_black);
		gc9a01_draw_rect(&textBox, gc9a01_color_white, 1);

		y += 16;
	}

	drawHomeScreenElementsStartBt();	
}

void setStateMachine(stateMachine_e newState){
	TIM1->CNT = 0x8000;
	lastEncoderState = TIM1->CNT;
	encoderCnt = 0;
	currentTrigTime = 0;

	if(newState == STATE_TRIG){
		triggerCamera(true);
	} else {
		triggerCamera(false);
	}

	if(state == STATE_STANDBY || newState == STATE_STANDBY){
		holdEncoderWaitForDepress = true;
		autoShutdownService();
	}

	if(state == STATE_SCREENSAVER || newState == STATE_SCREENSAVER){
		gc9a01_fill_screen(gc9a01_color_black);
	}

	state = newState;
	if(state != STATE_SCREENSAVER){
		drawHomeScreenElements();
		drawHomeScreenValuesAll();
	}

}

int main(void){
	char tmpS[16];
	int encoderDelta;			// delta of reading since last to current reading
	int encoderPostDivDelta;	// counter after dividing down the counter for not-so fine interval
	
	state = STATE_RESET;

	initMcu();

	autoShutdownTimer = 0;
	trigUpdateLcd = false;

	memset(&conf, 0, sizeof(conf));
	conf.timelapseInterval = 1.0;
	conf.shutterDelay = DEFAULT_SHUTTER_DELAY;
	conf.shutterSpeed = DEFAULT_SHUTTER_TIME;
	memset(&encoderBt, 0, sizeof(encoderBt));
	selectedElem = HOME_ELE_SHUTTER_TIME;

	selectTiaSens(0);

	gc9a01_init();

	gc9a01_fill_screen(gc9a01_color_black);

	// set halfway?
	TIM1->CNT = 0x8000;
	lastEncoderState = TIM1->CNT;

	TIM1->CR1 |= TIM_CR1_CEN;		// enable timer
	TIM6->CR1 |= TIM_CR1_CEN;		// enable timer
	TIM14->CR1 |= TIM_CR1_CEN;		// enable timer
	__enable_irq();

	state = STATE_STANDBY;
	drawHomeScreenElements();
	drawHomeScreenValuesAll();
	for(EVER){
		// record the delta rotary encoder counts if we rotated the encoder
		if(lastEncoderState != TIM1->CNT){
			encoderDelta = TIM1->CNT - lastEncoderState;
			lastEncoderState = TIM1->CNT;
			encoderCnt += encoderDelta;
			autoShutdownService();
		}

		switch(state){
			case STATE_STANDBY:
				if(autoShutdownTimer >= SCREENSAVER_INTERVAL){
					setStateMachine(STATE_SCREENSAVER);
				}
				if(encoderBt.pressedTrig == true){
					encoderBt.pressedTrig = false;
					if(selectedElem != HOME_ELE_START_BT){
						setStateMachine(STATE_EDITING_VAR);
					}
				}
				if(encoderBt.depressedTrig == true){
					holdEncoderWaitForDepress = false;
					encoderBt.depressedTrig = false;
					if(selectedElem == HOME_ELE_START_BT){
						drawHomeScreenElementsStartBt();
					}
				}
				if(!holdEncoderWaitForDepress && encoderBt.pressedDur){
					if(selectedElem == HOME_ELE_START_BT){
						drawHomeScreenElementsStartBtProg(encoderBt.pressedDur / (float)START_BUTTON_PRESS_DUR);
						if(encoderBt.pressedDur >= START_BUTTON_PRESS_DUR){
							setStateMachine(STATE_ARMED);
						}
					}
				}

				if(encoderCnt > 4){
					encoderCnt = 0;
					if(selectedElem != (HOME_ELE_END-1)){selectedElem++;}
					drawHomeScreenElements();
				}
				else if(encoderCnt < -4){
					encoderCnt = 0;
					if(selectedElem != 0){selectedElem--;}
					drawHomeScreenElements();
				}

				break;
			case STATE_EDITING_VAR:
				if(autoShutdownTimer >= SCREENSAVER_INTERVAL){
					setStateMachine(STATE_SCREENSAVER);
				}
				if(encoderBt.pressedTrig == true){
					encoderBt.pressedTrig = false;
					setStateMachine(STATE_STANDBY);
				}

				encoderPostDivDelta = 0;
				if(encoderCnt > 4){
					encoderCnt = 0;
					encoderPostDivDelta = 1;
				} else if(encoderCnt < -4){
					encoderCnt = 0;
					encoderPostDivDelta = -1;
				}
				if(encoderPostDivDelta){
					switch(selectedElem){
						case VAR_SHUTTER_TIME:
							conf.shutterSpeed += encoderPostDivDelta;
							if(conf.shutterSpeed < 1.0){conf.shutterSpeed = 1.0;}
							break;
						case VAR_TRIG_TIME:
							conf.shutterDelay += encoderPostDivDelta;
							if(conf.shutterDelay < 1.0){conf.shutterDelay = 1.0;}
							break;
						case VAR_TIMELAPSE_N:
							conf.timelapseNPics += encoderPostDivDelta;
							if(conf.timelapseNPics < -1){conf.timelapseNPics = -1;}
							break;
						case VAR_TIMELAPSE_DUR:
							conf.timelapseInterval += encoderPostDivDelta;
							if(conf.timelapseInterval < 1.0){conf.timelapseInterval = 1.0;}
							break;
						default:
							break;
					}
					drawHomeScreenValuesAll();
				}

				break;
			case STATE_ARMED:
				if(currentTrigTime > conf.shutterDelay){
					setStateMachine(STATE_TRIG);
				}
				else{
					drawHomeProgressUpdate(VAR_TRIG_TIME, currentTrigTime / conf.shutterDelay);
					// todo: bug where this, for a very small period of time, goes to zero
					npf_snprintf(tmpS, 16, "%5.2f", conf.shutterDelay - currentTrigTime);
					drawHomeScreenValuesSingle(VAR_TRIG_TIME, tmpS);
				}
				
				if(encoderBt.depressedTrig == true){
					holdEncoderWaitForDepress = false;
					encoderBt.depressedTrig = false;
					drawHomeScreenElementsStartBt();
				}

				if(!holdEncoderWaitForDepress && encoderBt.pressedDur){
					drawHomeScreenElementsStartBtProg(encoderBt.pressedDur / (float)STOP_BUTTON_PRESS_DUR);
					if(encoderBt.pressedDur >= STOP_BUTTON_PRESS_DUR){
						setStateMachine(STATE_STANDBY);
					}
				}
				break;
			case STATE_TRIG:
				if(currentTrigTime > conf.shutterSpeed){
					if(conf.timelapseNPics == -1){
						setStateMachine(STATE_TIMELAPSE);
					}
					else if(--conf.timelapseNPics < 0){
						setStateMachine(STATE_STANDBY);
					}
					else{
						setStateMachine(STATE_TIMELAPSE);
					}
				}
				else{
					drawHomeProgressUpdate(VAR_SHUTTER_TIME, currentTrigTime / conf.shutterSpeed);
					npf_snprintf(tmpS, 16, "%5.2f", conf.shutterSpeed - currentTrigTime);
					drawHomeScreenValuesSingle(VAR_SHUTTER_TIME, tmpS);
				}

				if(encoderBt.depressedTrig == true){
					holdEncoderWaitForDepress = false;
					encoderBt.depressedTrig = false;
					drawHomeScreenElementsStartBt();
				}

				if(!holdEncoderWaitForDepress && encoderBt.pressedDur){
					drawHomeScreenElementsStartBtProg(encoderBt.pressedDur / (float)STOP_BUTTON_PRESS_DUR);
					if(encoderBt.pressedDur >= STOP_BUTTON_PRESS_DUR){
						setStateMachine(STATE_STANDBY);
					}
				}
				break;
			case STATE_TIMELAPSE:
				if(currentTrigTime > conf.timelapseInterval){
					setStateMachine(STATE_TRIG);
				}
				else{		// update as needed
					drawHomeProgressUpdate(VAR_TIMELAPSE_DUR, currentTrigTime / conf.timelapseInterval);
					npf_snprintf(tmpS, 16, "%5.2f", conf.timelapseInterval - currentTrigTime);
					drawHomeScreenValuesSingle(VAR_TIMELAPSE_DUR, tmpS);
				}

				if(encoderBt.depressedTrig == true){
					holdEncoderWaitForDepress = false;
					encoderBt.depressedTrig = false;
					drawHomeScreenElementsStartBt();
				}

				if(!holdEncoderWaitForDepress && encoderBt.pressedDur){
					drawHomeScreenElementsStartBtProg(encoderBt.pressedDur / (float)STOP_BUTTON_PRESS_DUR);
					if(encoderBt.pressedDur >= STOP_BUTTON_PRESS_DUR){
						setStateMachine(STATE_STANDBY);
					}
				}
				break;
			case STATE_SCREENSAVER:
				if(trigUpdateLcd){		// update as needed
					trigUpdateLcd = false;
					serviceScreenSaver();
				}
				if(autoShutdownTimer >= AUTO_SHUTDOWN_INTERVAL){
					// setStateMachine(STATE_SHUTDOWN);		// explicit state transition not needed, end of device
					shutdownDevice();
				}
				if(encoderBt.pressedTrig == true){
					encoderBt.pressedTrig = false;
					setStateMachine(STATE_STANDBY);
				}
				if(encoderCnt){
					setStateMachine(STATE_STANDBY);
				}
				break;
			default:
				break;
		}
	}

	return 0;
}

/*
 * real time interrupt triggered by TIM14
 * this runs every 1Khz
 */
void TIM14_IRQHandler(void){
	static uint lastLcdUpdate = 0;			// last time since we updated the lcd
	uint buttonState;

	TIM14->SR = 0;		// clear pending interrupt

	if(++lastLcdUpdate >= 50){
		trigUpdateLcd = true;
		lastLcdUpdate = 0;
	}

	// if we pressed the encoder button
	buttonState = (GPIOB->IDR & (1 << 1)) == 0;		// true if pressed, false if not
	buttonPressHandler(&encoderBt, buttonState);

	buttonState = (GPIOA->IDR & (1 << 4)) == 0;
	buttonPressHandler(&leftBt, buttonState);

	buttonState = (GPIOA->IDR & (1 << 5)) == 0;
	buttonPressHandler(&rightBt, buttonState);

	currentTrigTime += 0.001;	
}

// this gets called inside the 1Khz IQR to handle button debounding
void buttonPressHandler(struct button_s *bt, uint buttonState){
	if(bt->validCnt){
		if(--bt->validCnt == 0){
			// by this time a button was held at some state. Check if it's the same as what initiated it
			if(!(buttonState ^ bt->preValidState)){
				if(buttonState){
					bt->pressedTrig = true;
					bt->depressedTrig = false;
				}
				else{
					bt->pressedTrig = false;
					bt->depressedTrig = true;
				}
				bt->pressedDur = 0;
				autoShutdownService();
			}
		}
	}
	else{
		if(buttonState ^ bt->lastState){		// edge trigger
			bt->validCnt = BUTTON_VALID_PERIOD_TICK;
			bt->preValidState = buttonState;
		}
		else if(buttonState){		// if are held high for 
			bt->pressedDur++;
			autoShutdownService();
		}
	}
	bt->lastState = buttonState;
}

// called once every 10 seconds
void TIM6_DAC_LPTIM1_IRQHandler(void){
	TIM6->SR = 0;	// clear pending interrupt
	autoShutdownTimer += 1;
}

// service to clear the auto-shutdown timer
void autoShutdownService(void){
	autoShutdownTimer = 0;
}

void shutdownDevice(void){
	GPIOA->BRR = 1 << PIN_PSU_ENABLE;
}

/**
 * Selects the amplification amount from the TransImpedance Amplifier circuit
 * Range 0 (disabled) to 3 (max amp)
 */
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