#ifndef MAIN_H
#define MAIN_H

#include "config.h"

typedef enum{
	STATE_HOME,				// home screen
	STATE_TIMELAPSE_SUB,	// timelapse trigger sub-state
	STATE_SETTINGS_SUB,		// settings sub-state
	STATE_SCREENSAVER,		// screensaver state
}topStates_e;

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

struct settings_s{
    uint32 brightness;
    uint32 redMode;
    int32 screenSaverDurIdx;
};

extern uint32 fgColor;
extern uint32 bgColor;
extern uint autoShutdownTimer;
extern bool trigUpdateLcd;

extern struct button_s encoderBt;

extern bool holdEncoderWaitForDepress;
extern int encoderCnt;
extern int encoderDelta;			// delta of reading since last to current reading
extern int encoderPostDivDelta;	// counter after dividing down the counter for not-so fine interval

extern float currentTrigTime;

extern struct settings_s set;

void resetEncoders(void);
void setStateMachine(topStates_e newState);
void setStateMachinePrev(void);
void triggerCamera(bool trig);
void autoShutdownService(void);
void shutdownDevice(void);

#endif