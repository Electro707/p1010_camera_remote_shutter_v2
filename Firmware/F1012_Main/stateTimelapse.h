#ifndef STATE_TIMELAPSE_H
#define STATE_TIMELAPSE_H

#include "config.h"

// run-time modes
typedef enum{
	TL_STATE_STANDBY,		// standby, not doing anything with pictures
	TL_STATE_EDITING_VAR,	// editing variable in UI
	TL_STATE_ARMED,		// armed, waiting to take picture
	TL_STATE_TRIG,			// camera triggered
	TL_STATE_TIMELAPSE,	// doing timelapse
}timelapseStates_e;

// home screen elements. must be in the same order as what is drawn
typedef enum{
	TL_ELE_TRIG_TIME = 0,
	TL_ELE_SHUTTER_TIME,
	TL_ELE_TIMELAPSE_N,
	TL_ELE_TIMELAPSE_DUR,		// end of editable variables
	TL_ELE_START_BT,
	TL_ELE_BACK,
	TL_ELE_END,
}timelapseElements_e;
#define TIMELAPSE_ELE_NUMB_END (TL_ELE_TIMELAPSE_DUR+1)

// trigger configs
struct timelapseConfig_s{
	float shutterDelay;			// sec, the time from trigger to shutter open
	float shutterSpeed;			// sec, the shutter speed
	int timelapseNPics;		    // n, number of pictures to take in trigger time. Set to -1 for infinite
	float timelapseInterval;	// sec, timelapse interval between different pictures 
};

void stateTimelapseInit(void);
void enterTimelapseState(void);
void serviceTimelapseState(void);

#endif