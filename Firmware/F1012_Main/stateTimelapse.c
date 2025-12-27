#include <stddef.h>
#include <string.h>
#include "nanoprintf.h"

#include "stateTimelapse.h"
#include "main.h"
#include "graphics.h"


timelapseStates_e timelapseSubState;
timelapseElements_e selectedElem;		// selected element on screen, can be button or editable variable

struct timelapseConfig_s timelapseConf;
uint prevNPics;				// part of the previous config that is blown away during runtime, so this is used to restore it

const char *varTexts[TIMELAPSE_ELE_NUMB_END] = {"Shutter delay:", "Shutter Time:", "Timelapse #Pics:", "Timelapse Delay:"};

void drawHomeScreenValuesSingle(int index, char *toWrite){
	uint y = 50 + 32*index;

	// select if highlighted based off if selected
	if(timelapseSubState == TL_STATE_EDITING_VAR && selectedElem == index){
		uiInvertBgFg();
	}

	uiSetAlignment(ALIGN_RIGHT);
	uiPrintText(toWrite, 205, y, NULL);

	if(timelapseSubState == TL_STATE_EDITING_VAR && selectedElem == index){
		uiInvertBgFg();
	}
}

void drawTimelapseSubstateAllVals(void){
	char nStr[10];
	int varI;
	float varF;
	uint y = 50;

	uiSetFontSize(FONT_SIZE_SMALL);
	uiSetAlignment(ALIGN_RIGHT);

	for(int i=0;i<TIMELAPSE_ELE_NUMB_END;i++){
		if(timelapseSubState == TL_STATE_EDITING_VAR && selectedElem == i){
			uiInvertBgFg();
		}

		switch(i){
			case TL_ELE_TRIG_TIME:
				varF = timelapseConf.shutterDelay;
				npf_snprintf(nStr, 10, "%5.2f", varF);
				break;
			case TL_ELE_SHUTTER_TIME:
				varF = timelapseConf.shutterSpeed;
				npf_snprintf(nStr, 10, "%5.2f", varF);
				break;
			case TL_ELE_TIMELAPSE_N:
				varI = timelapseConf.timelapseNPics;
				npf_snprintf(nStr, 10, "%5d", varI);
				break;
			case TL_ELE_TIMELAPSE_DUR:
				varF = timelapseConf.timelapseInterval;
				npf_snprintf(nStr, 10, "%5.2f", varF);
				break;
			default:
				break;
		}

		uiPrintText(nStr, 205, y, NULL);
		y += 32;

		if(timelapseSubState == TL_STATE_EDITING_VAR && selectedElem == i){
			uiInvertBgFg();
		}
	}
}

void drawTimelapseStartBt(float prog){
	uiSetRectangleSize(64, 36);
	// uiSetDrawPos((UI_WIDTH-64)/2, 190);
	uiSetDrawPos(50, 190);
	switch(timelapseSubState){
		case TL_STATE_STANDBY:
		case TL_STATE_EDITING_VAR:
			if(selectedElem == TL_ELE_START_BT){
                if(set.redMode){
                    uiSetForeground(0xa000);        // color: #a50000
                } else {
                    uiSetForeground(0x57eb);        // color: #52ff5a
                }
				
			} else {
                if(!set.redMode){
                    uiSetForeground(0xed23);        // color: #efa619
                }
			}
			uiDrawButton("START", prog);
			break;
		case TL_STATE_TRIG:
		case TL_STATE_ARMED:
		case TL_STATE_TIMELAPSE:
			uiSetForeground(0xfb28);
			uiDrawButton("STOP", prog);
			break;
		default:
			break;
	}
	// restore default foreground
	uiSetForeground(fgColor);
}

void drawTimelapseBackBt(float prog){
	uiSetRectangleSize(64, 36);
	// uiSetDrawPos((UI_WIDTH-64)/2, 190);
	uiSetDrawPos((UI_WIDTH-64-50), 190);
	uiSetForeground(fgColor);

	if(selectedElem == TL_ELE_BACK){
		uiSetForeground(0x57eb);
	} else {
		uiSetForeground(fgColor);
	}

	uiDrawButton("BACK", prog);
	uiSetForeground(fgColor);
}

// updates a certain progress bar. prog is from 0 to 1
void drawTimelapseProgUpdate(timelapseElements_e config, float prog){
	uint y = 50+16+(config*32);

	uiSetRectangleThickness(1);
	uiSetDrawPos(50, y+4);
	uiSetRectangleSize(140, 8);

	uiDrawSlider(prog);
}

void drawTimelapseScreen(void){
	uint y = 50;

	uiSetFontSize(FONT_SIZE_SMALL);
	uiSetAlignment(ALIGN_LEFT);

	uiSetRectangleThickness(1);
	uiSetRectangleSize(140, 8);

	for(int i=0;i<TIMELAPSE_ELE_NUMB_END;i++){
		if(timelapseSubState == TL_STATE_STANDBY && selectedElem == i){
			uiInvertBgFg();
		}

		uiPrintText(varTexts[i], 35, y, NULL);
		y += 16;

		if(timelapseSubState == TL_STATE_STANDBY && selectedElem == i){
			uiInvertBgFg();
		}

		uiSetDrawPos(50, y+4);
		
		uiDrawSlider(0.0);

		y += 16;
	}

	drawTimelapseStartBt(0);
	drawTimelapseBackBt(0);
}

void setTimelapseSubstate(timelapseStates_e newState){
	resetEncoders();
	currentTrigTime = 0;

	//this enables the camera trigger on transition
	if(newState == TL_STATE_TRIG){
		triggerCamera(true);
	} else {
		triggerCamera(false);
	}

	// set a hold encoder flag and service auto shutdown when we are transitioning in or out of standby
	if(timelapseSubState == TL_STATE_STANDBY || newState == TL_STATE_STANDBY){
		holdEncoderWaitForDepress = true;
		autoShutdownService();
	}

	if(newState == TL_STATE_ARMED){
		prevNPics = timelapseConf.timelapseNPics;
	}

	// if we already started taking pictures, restore the last timelapse amount on exit back to standby
	if(newState == TL_STATE_STANDBY && (timelapseSubState == TL_STATE_TRIG || timelapseSubState == TL_STATE_TIMELAPSE)){
		if(prevNPics != 0xA5A5A5A5){
			timelapseConf.timelapseNPics = prevNPics;
		}
	}

	timelapseSubState = newState;
	drawTimelapseScreen();
	drawTimelapseSubstateAllVals();
}

void enterTimelapseState(void){
    selectedElem = TL_ELE_SHUTTER_TIME;
    setTimelapseSubstate(TL_STATE_STANDBY);
}

void stateTimelapseInit(void){
    // store last config in nvm if possible
	memset(&timelapseConf, 0, sizeof(timelapseConf));
	timelapseConf.timelapseInterval = DEFAULT_TIMELAPSE_DELAY;
	timelapseConf.shutterDelay = DEFAULT_SHUTTER_DELAY;
	timelapseConf.shutterSpeed = DEFAULT_SHUTTER_TIME;
	prevNPics = 0xA5A5A5A5;		// key that this is None/not populated
	memset(&encoderBt, 0, sizeof(encoderBt));

    timelapseSubState = TL_STATE_STANDBY;
}

void serviceTimelapseState(void){
	char tmpS[16];

	switch(timelapseSubState){
		case TL_STATE_STANDBY:
			if(autoShutdownTimer >= SCREENSAVER_INTERVAL){
				setStateMachine(STATE_SCREENSAVER);
                return;
			}
			if(encoderBt.pressedTrig == true){
				encoderBt.pressedTrig = false;
				if(selectedElem < TIMELAPSE_ELE_NUMB_END){
					setTimelapseSubstate(TL_STATE_EDITING_VAR);
                    return;
				}
                else if(selectedElem == TL_ELE_BACK){
                    setStateMachine(STATE_HOME);
                    return;
                }
			}
			if(encoderBt.depressedTrig == true){
				holdEncoderWaitForDepress = false;
				encoderBt.depressedTrig = false;
				if(selectedElem == TL_ELE_START_BT){
					drawTimelapseStartBt(0);
				}
			}
			if(!holdEncoderWaitForDepress && encoderBt.pressedDur){
				if(selectedElem == TL_ELE_START_BT){
					drawTimelapseStartBt(encoderBt.pressedDur / (float)START_BUTTON_PRESS_DUR);
					if(encoderBt.pressedDur >= START_BUTTON_PRESS_DUR){
						setTimelapseSubstate(TL_STATE_ARMED);
                        return;
					}
				}
			}

			if(encoderCnt > 4){
				encoderCnt = 0;
				if(selectedElem != (TL_ELE_END-1)){selectedElem++;}
				drawTimelapseScreen();
			}
			else if(encoderCnt < -4){
				encoderCnt = 0;
				if(selectedElem != 0){selectedElem--;}
				drawTimelapseScreen();
			}

			break;
		case TL_STATE_EDITING_VAR:
			if(autoShutdownTimer >= SCREENSAVER_INTERVAL){
				setStateMachine(STATE_SCREENSAVER);
                return;
			}
			if(encoderBt.pressedTrig == true){
				encoderBt.pressedTrig = false;
				setTimelapseSubstate(TL_STATE_STANDBY);
                return;
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
					case TL_ELE_TRIG_TIME:
						timelapseConf.shutterDelay += encoderPostDivDelta;
						if(timelapseConf.shutterDelay < 1.0){timelapseConf.shutterDelay = 1.0;}
						break;
					case TL_ELE_SHUTTER_TIME:
						timelapseConf.shutterSpeed += encoderPostDivDelta;
						if(timelapseConf.shutterSpeed < 1.0){timelapseConf.shutterSpeed = 1.0;}
						break;
					case TL_ELE_TIMELAPSE_N:
						timelapseConf.timelapseNPics += encoderPostDivDelta;
						if(timelapseConf.timelapseNPics < -1){timelapseConf.timelapseNPics = -1;}
						break;
					case TL_ELE_TIMELAPSE_DUR:
						timelapseConf.timelapseInterval += encoderPostDivDelta;
						if(timelapseConf.timelapseInterval < 1.0){timelapseConf.timelapseInterval = 1.0;}
						break;
					default:
						break;
				}
				drawTimelapseSubstateAllVals();
			}

			break;
		case TL_STATE_ARMED:
			if(currentTrigTime > timelapseConf.shutterDelay){
				setTimelapseSubstate(TL_STATE_TRIG);
			}
			else{
				drawTimelapseProgUpdate(TL_ELE_TRIG_TIME, currentTrigTime / timelapseConf.shutterDelay);
				// todo: bug where this, for a very small period of time, goes to zero
				npf_snprintf(tmpS, 16, "%5.2f", timelapseConf.shutterDelay - currentTrigTime);
				drawHomeScreenValuesSingle(TL_ELE_TRIG_TIME, tmpS);
			}
			
			if(encoderBt.depressedTrig == true){
				holdEncoderWaitForDepress = false;
				encoderBt.depressedTrig = false;
				drawTimelapseStartBt(0);
			}

			if(!holdEncoderWaitForDepress && encoderBt.pressedDur){
				drawTimelapseStartBt(encoderBt.pressedDur / (float)STOP_BUTTON_PRESS_DUR);
				if(encoderBt.pressedDur >= STOP_BUTTON_PRESS_DUR){
					setTimelapseSubstate(TL_STATE_STANDBY);
				}
			}
			break;
		case TL_STATE_TRIG:
			if(currentTrigTime > timelapseConf.shutterSpeed){
				if(timelapseConf.timelapseNPics == -1){
					setTimelapseSubstate(TL_STATE_TIMELAPSE);
				}
				else if(--timelapseConf.timelapseNPics < 0){
					setTimelapseSubstate(TL_STATE_STANDBY);
				}
				else{
					setTimelapseSubstate(TL_STATE_TIMELAPSE);
				}
			}
			else{
				drawTimelapseProgUpdate(TL_ELE_SHUTTER_TIME, currentTrigTime / timelapseConf.shutterSpeed);
				npf_snprintf(tmpS, 16, "%5.2f", timelapseConf.shutterSpeed - currentTrigTime);
				drawHomeScreenValuesSingle(TL_ELE_SHUTTER_TIME, tmpS);
			}

			if(encoderBt.depressedTrig == true){
				holdEncoderWaitForDepress = false;
				encoderBt.depressedTrig = false;
				drawTimelapseStartBt(0.0);
			}

			if(!holdEncoderWaitForDepress && encoderBt.pressedDur){
				drawTimelapseStartBt(encoderBt.pressedDur / (float)STOP_BUTTON_PRESS_DUR);
				if(encoderBt.pressedDur >= STOP_BUTTON_PRESS_DUR){
					setTimelapseSubstate(TL_STATE_STANDBY);
				}
			}
			break;
		case TL_STATE_TIMELAPSE:
			if(currentTrigTime > timelapseConf.timelapseInterval){
				setTimelapseSubstate(TL_STATE_TRIG);
			}
			else{		// update as needed
				drawTimelapseProgUpdate(TL_ELE_TIMELAPSE_DUR, currentTrigTime / timelapseConf.timelapseInterval);
				npf_snprintf(tmpS, 16, "%5.2f", timelapseConf.timelapseInterval - currentTrigTime);
				drawHomeScreenValuesSingle(TL_ELE_TIMELAPSE_DUR, tmpS);
			}

			if(encoderBt.depressedTrig == true){
				holdEncoderWaitForDepress = false;
				encoderBt.depressedTrig = false;
				drawTimelapseStartBt(0.0);
			}

			if(!holdEncoderWaitForDepress && encoderBt.pressedDur){
				drawTimelapseStartBt(encoderBt.pressedDur / (float)STOP_BUTTON_PRESS_DUR);
				if(encoderBt.pressedDur >= STOP_BUTTON_PRESS_DUR){
					setTimelapseSubstate(TL_STATE_STANDBY);
				}
			}
			break;
		default:
			break;
	}
}