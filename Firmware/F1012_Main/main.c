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
#include <stm32g071xx.h>
#include <string.h>
#include "nanoprintf.h"
#include "graphics.h"
#include "font.h"
#include "bitmap.h"
#include "stateTimelapse.h"
#include "stateScreensaver.h"
#include "main.h"


void buttonPressHandler(struct button_s *bt, uint buttonState);
void selectTiaSens(uint8_t sens);
void line_draw_abstract(uint8_t *canvasBuff, uint16_t canvasW, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void line_draw_vert_abstract(uint8_t *canvasBuff, uint16_t canvasW, uint16_t x, uint16_t h);
void trigBattMonReading(void);

typedef enum{
	HOME_ELE_TIMELAPSE = 0,
	HOME_ELE_SETTINGS,
	HOME_ELE_END,
}homeElements_e;
const char *homeVarText[HOME_ELE_END] = {"Timelapse Mode", "Settings"};

typedef enum{
	SETT_ELE_BRIGHTNESS = 0,
	SETT_ELE_RED_MODE,
	SETT_ELE_SS_DUR,	// end of editable variables
	SETT_ELE_BACK,
	SETT_ELE_END,
}settingsElements_e;
#define SETTINGS_ELE_NUMB_END (SETT_ELE_SS_DUR+1)
const char *settingVarTxt[SETTINGS_ELE_NUMB_END] = {"Brightness", "Red Mode", "Screensaver Dur"};
#define N_SCREENSAVER_DUR	4
const uint32 settSSDur[N_SCREENSAVER_DUR] = {10, 30, 60, 120};

topStates_e state;
topStates_e prevState;

uint autoShutdownTimer;		// counter for auto shutting down

bool trigUpdateLcd;			// whether main should update something on the display

uint lastEncoderState;
int encoderCnt = 0;								// local counter for the encoder position
bool holdEncoderWaitForDepress = false;			// flag if we are waiting for the encoder to depress to not count previous hold state against counter

uint battMonTick = 0;		// isr ticks to determine when to measure battery monitor, once a second
bool battMonTriggered = false;

struct button_s encoderBt;
struct button_s leftBt;
struct button_s rightBt;

int encoderDelta;			// delta of reading since last to current reading
int encoderPostDivDelta;	// counter after dividing down the counter for not-so fine interval

float currentTrigTime;		// ticks, current state timer for trigger and arm
							// this is written directly in interrupt

uint32 fgColor;
uint32 bgColor;

homeElements_e homeSelected;

uint32 isSettingsEdit;		// settings doesn't really need a full state machine, a bool is enough
settingsElements_e settingsSelected;

struct settings_s set;

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

	// PA2 (CAM_FOC) - output
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
	GPIOA->MODER = 0x695a50df;
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

	// init adc
    RCC->APBENR2 |= RCC_APBENR2_ADCEN;
    // Disable ADC in case it was beforehand
    if(ADC1->CR & ADC_CR_ADEN){
        ADC1->CR |= ADC_CR_ADDIS;
        while(ADC1->CR & ADC_CR_ADDIS);
    }
	ADC1->CFGR2 = 0x40000000;       // PCLK/2
    ADC1->SMPR = (0b11 << ADC_SMPR_SMP1_Pos);   // 12.5 ADC clock cycles for sampling time
    ADC1->CHSELR = 1 << 13;          // vrefint	// todo: make run-time settable for other ADC inputs when that gets added
	ADC->CCR  = (1 << 22);		 	// enable vref
    ADC1->CR = ADC_CR_ADVREGEN;      // ADVREGEN=1

	// set led backlight on
	GPIOA->ODR |= 1 << 15;
	DISP_CS_1;

	NVIC_EnableIRQ(TIM6_DAC_LPTIM1_IRQn);
	NVIC_EnableIRQ(TIM14_IRQn);

	SPI2->CR1 |= SPI_CR1_SPE;
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

void drawBatteryVoltage(float batt){
	// for now is just text
	// char nStr[16];
	// npf_snprintf(nStr, 16, "%5.2f v", batt);
	// todo: this
	// uiSetAlignment(ALIGN_CENTER);
	// uiSetFontSize(FONT_SIZE_SMALL);

	uiSetDrawPos(104, 20);

	if(LL_GPIO_IsOutputPinSet(GPIOC, BATT_CHR_Pin) == 0){
		uiSetForeground(gc9a01_color_green);
		uiDrawCanvas(batteryCharge, IMG_W_batteryCharge, IMG_H_batteryCharge);
	}
	else if(LL_GPIO_IsOutputPinSet(GPIOC, BATT_PGOOD_Pin) == 0){
		uiSetForeground(gc9a01_color_red);
		uiDrawCanvas(batteryBase, IMG_W_batteryBase, IMG_H_batteryBase);
	}
	else{
		uiSetForeground(fgColor);
		uiDrawCanvas(batteryBase, IMG_W_batteryBase, IMG_H_batteryBase);
		// todo: include progress as rectangle inside this
	}

	uiSetForeground(fgColor);

	// uiPrintText(nStr, 120, 20, NULL);
	// uiSetForeground(gc9a01_color_white);
}

void resetEncoders(void){
	TIM1->CNT = 0x8000;
	lastEncoderState = TIM1->CNT;
	encoderCnt = 0;
}

void drawHomeScreen(void){
	uint32 y = 50;
	uiSetFontSize(FONT_SIZE_MED);
	uiSetRectangleSize(200, 32);

	for(int i=0;i<HOME_ELE_END;i++){
		uiSetDrawPos(20, y);
		if(homeSelected == i){
			uiInvertBgFg();
		}
		uiDrawButton(homeVarText[i], 0);
		if(homeSelected == i){
			uiInvertBgFg();
		}
		y += 50;
	}
}

void serviceHomeState(void){
	if(autoShutdownTimer >= SCREENSAVER_INTERVAL){
		setStateMachine(STATE_SCREENSAVER);
		return;
	}

	if(encoderCnt > 4){
		encoderCnt = 0;
		if(homeSelected != (HOME_ELE_END-1)){homeSelected++;}
		drawHomeScreen();
	}
	else if(encoderCnt < -4){
		encoderCnt = 0;
		if(homeSelected != 0){homeSelected--;}
		drawHomeScreen();
	}

	if(encoderBt.pressedTrig == true){
		encoderBt.pressedTrig = false;
		if(homeSelected == HOME_ELE_TIMELAPSE){
			setStateMachine(STATE_TIMELAPSE_SUB);
			return;
		}
		else if(homeSelected == HOME_ELE_SETTINGS){
			setStateMachine(STATE_SETTINGS_SUB);
			return;
		}
	}
}

void drawSettingsScreenElement(void){
	char nStr[16];

	uiSetFontSize(FONT_SIZE_SMALL);
	uiSetAlignment(ALIGN_LEFT);
	uiSetRectangleSize(75, 16);
	uiSetDrawPos(135, 50);
	uiSetRectangleThickness(1);
	uiDrawSlider(0);

	if(set.redMode){
		uiInvertBgFg();
	}
	uiPrintText("YES", 135, 82, NULL);
	if(!set.redMode){
		uiInvertBgFg();
	}
	uiPrintText("NO", 194, 82, NULL);
	uiInvertBgFg();


	uiSetAlignment(ALIGN_RIGHT);
	uiSetRectangleSize(60, 18);
	uiSetDrawPos(150, 112);
	if(isSettingsEdit && settingsSelected == SETT_ELE_SS_DUR) uiInvertBgFg();
	
	npf_snprintf(nStr, 16, "% 3d sec", settSSDur[set.screenSaverDurIdx]);
	uiPrintText(nStr, 208, 114, NULL);
	uiSetRectangleThickness(1);
	if(isSettingsEdit && settingsSelected == SETT_ELE_SS_DUR) uiInvertBgFg();
	uiDrawRectOutline();
}

void drawSettingsScreenLabel(void){
	uint32 y = 50;
	uiSetFontSize(FONT_SIZE_SMALL);
	uiSetAlignment(ALIGN_LEFT);
	uiSetRectangleSize(200, 32);

	for(int i=0;i<SETTINGS_ELE_NUMB_END;i++){
		if(!isSettingsEdit && settingsSelected == i) uiInvertBgFg();
		uiPrintText(settingVarTxt[i], 25, y, NULL);
		if(!isSettingsEdit && settingsSelected == i) uiInvertBgFg();
		y += 32;
	}

	uiSetRectangleSize(64, 36);
	uiSetDrawPos((UI_WIDTH-64)/2, 190);
	if(settingsSelected == SETT_ELE_BACK)		uiSetForeground(0x57eb);
	uiDrawButton("BACK", 0.0);
	// restore default foreground
	uiSetForeground(fgColor);

}

void serviceSettingsState(){
	if(autoShutdownTimer >= SCREENSAVER_INTERVAL){
		setStateMachine(STATE_SCREENSAVER);
		return;
	}

	if(isSettingsEdit == false){
		if(encoderCnt > 4){
			encoderCnt = 0;
			if(settingsSelected != (SETT_ELE_END-1)){settingsSelected++;}
			drawSettingsScreenLabel();
		}
		else if(encoderCnt < -4){
			encoderCnt = 0;
			if(settingsSelected != 0){settingsSelected--;}
			drawSettingsScreenLabel();
		}
	}
	else {
		encoderPostDivDelta = 0;
		if(encoderCnt > 4){
			encoderCnt = 0;
			encoderPostDivDelta = 1;
		} else if(encoderCnt < -4){
			encoderCnt = 0;
			encoderPostDivDelta = -1;
		}
		if(encoderPostDivDelta){
			switch(settingsSelected){
				case SETT_ELE_SS_DUR:
					set.screenSaverDurIdx += encoderPostDivDelta;
					if(set.screenSaverDurIdx < 0){set.screenSaverDurIdx = 0;}
					if(set.screenSaverDurIdx >= N_SCREENSAVER_DUR){set.screenSaverDurIdx = N_SCREENSAVER_DUR-1;}
					drawSettingsScreenElement();
					break;
				default:
					break;
			}
		}

	}

	if(encoderBt.pressedTrig == true){
		encoderBt.pressedTrig = false;

		if(settingsSelected == SETT_ELE_BACK){
			setStateMachine(STATE_HOME);
			return;
		}

		if(isSettingsEdit){
			isSettingsEdit = false;
			drawSettingsScreenLabel();
			drawSettingsScreenElement();
		} else {
			isSettingsEdit = true;
			drawSettingsScreenLabel();
			drawSettingsScreenElement();
		}
	}
}

void setStateMachine(topStates_e newState){
	resetEncoders();

	uiFillScreenBg();
	switch(newState){
		case STATE_HOME:
			homeSelected = HOME_ELE_TIMELAPSE;
			drawHomeScreen();
			break;
		case STATE_TIMELAPSE_SUB:
			enterTimelapseState();
			break;
		case STATE_SETTINGS_SUB:
			isSettingsEdit = false;
			settingsSelected = 0;
			drawSettingsScreenLabel();
			drawSettingsScreenElement();
			break;
		case STATE_SCREENSAVER:
			// no need for screen update as the service will contionusally update it anyways
			break;	
		default:
			break;
	}

	prevState = state;
	state = newState;
}

void setStateMachinePrev(void){
	setStateMachine(prevState);
}

int main(void){
	initMcu();
	adcCal();

	// calculate adc vrefint votltage. See section 3.14.2 of datasheet for magic location
	uint16_t adcVrefIntCnt = *(uint16_t *)0x1FFF75AA;
	float adcVrefIntV = ((float)adcVrefIntCnt / 4096.0) * 3.0;

	autoShutdownTimer = 0;
	trigUpdateLcd = false;

	// store last config in nvm if possible
	stateTimelapseInit();

	selectTiaSens(0);

	gc9a01Init();

	fgColor = gc9a01_color_white;
	bgColor = gc9a01_color_black;

	set.brightness = 255;
	set.redMode = false;

	if(set.redMode){
		fgColor = gc9a01_color_red;
	}

	uiSetBackground(bgColor);
	uiSetForeground(fgColor);
	// uiFillScreenBg();		// note: done on state transition before forever loop

	TIM1->CR1 |= TIM_CR1_CEN;		// enable timer for encoder
	TIM6->CR1 |= TIM_CR1_CEN;		// enable timer for shutdown timer
	TIM14->CR1 |= TIM_CR1_CEN;		// enable timer for real time
	__enable_irq();

	ADC1->CR |= ADC_CR_ADEN;       					// enable ADC
	while((ADC1->ISR & ADC_ISR_ADRDY) == 0);		// wait for adc to come up

	// initial trigger to have something to draw
	trigBattMonReading();
	battMonTriggered = true;

	setStateMachine(STATE_HOME);
	for(EVER){
		// record the delta rotary encoder counts if we rotated the encoder
		if(lastEncoderState != TIM1->CNT){
			encoderDelta = TIM1->CNT - lastEncoderState;
			lastEncoderState = TIM1->CNT;
			encoderCnt += encoderDelta;
			autoShutdownService();
		}

		// The state machine handler for the timelapse screen
		switch(state){
			case STATE_HOME:
				serviceHomeState();
				break;
			case STATE_TIMELAPSE_SUB:
				serviceTimelapseState();
				break;
			case STATE_SETTINGS_SUB:
				serviceSettingsState();
				break;
			case STATE_SCREENSAVER:
				serviceScreenSaverState();
				break;
			default:
				break;
		}
		

		if(battMonTriggered){
			if(ADC1->ISR & ADC_ISR_EOC){
				float meas = (float)(ADC1->DR);
				meas /= 4096;
				meas = adcVrefIntV / meas;
				
				if(state != STATE_SCREENSAVER){
					// just update the display
					drawBatteryVoltage(meas);
				}

				battMonTriggered = false;
			}
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

	if(++lastLcdUpdate >= LCD_UPDATE_RATE_MS){
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

	if(++battMonTick >= BATTERY_INDICATOR_UPDATE_RATE){		// once every 2 seconds should be OK
		battMonTick = 0;
		trigBattMonReading();
		battMonTriggered = true;
	}

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

void trigBattMonReading(void){
	ADC1->CR |= ADC_CR_ADSTART;
}

void triggerCamera(bool trig){
	volatile uint32_t *reg;
	if(trig){
		reg = &GPIOA->BSRR;
	} else {
		reg = &GPIOA->BRR;
	}

	*reg = (1 << 7) | (1 << 2);
}