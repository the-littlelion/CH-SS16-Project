/*
 * JoyArduinoHandler.h
 *
 *  Created on: 05. Juni 2016
 *      Author: theoneone
 */

#ifndef JOYSTICKCOMPETITIONPRO_H_
#define JOYSTICKCOMPETITIONPRO_H_

#include <Windows.h>

/**
 * single hard-coded device
 * TODO. do it in a generic way - some changes may be necessary to maintain multiple devices
 * TODO. create automatic detection of joystick device.
 */
#if (defined(UNICODE) || defined (_UNICODE))
#define JOY_DEVICE L"\\\\.\\COM12"
#else
#define JOY_DEVICE "\\\\.\\COM12"
#endif

typedef struct _joystate {
	bool forward;
	bool back;
	bool left;
	bool right;
	bool fire;
} joystate;

class JoystickCompetitionPro {
public:
	JoystickCompetitionPro();
	virtual ~JoystickCompetitionPro();
	bool isConnected();
	joystate getState();

private:
    static DWORD dwThreadId;
    static HANDLE updaterThread;
    static HANDLE mtx;
	static HANDLE device;
	static bool done;

	static bool connected;
	static joystate state;

	static int objectCounter(int count);
	static DWORD WINAPI updateValues(void*);
	static void initUpdateThread();
	static void closeUpdateThread();
};

#endif /* JOYSTICKCOMPETITIONPRO_H_ */
