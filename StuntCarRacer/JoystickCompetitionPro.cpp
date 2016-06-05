/*
 * JoyArduinoHandler.cpp
 *
 *  Created on: 05. Juni 2016
 *      Author: root
 */

#include <iostream>
#include "JoystickCompetitionPro.h"

DWORD JoystickCompetitionPro::dwThreadId;
HANDLE JoystickCompetitionPro::updaterThread;
HANDLE JoystickCompetitionPro::mtx;
HANDLE JoystickCompetitionPro::device;
bool JoystickCompetitionPro::done = false;;

bool JoystickCompetitionPro::connected = false;
joystate JoystickCompetitionPro::state = {0};

JoystickCompetitionPro::JoystickCompetitionPro() {
	initUpdateThread();

}

JoystickCompetitionPro::~JoystickCompetitionPro() {
	closeUpdateThread();
}

bool JoystickCompetitionPro::isConnected() {
	return connected;
}
joystate JoystickCompetitionPro::getState() {
	return state;
}

DWORD WINAPI JoystickCompetitionPro::updateValues(void*) {
	// bitmasks for the joystate structure
#define JOY_FWD    1
#define JOY_BACK   2
#define JOY_LEFT   4
#define JOY_RIGHT  8
#define JOY_FIRE  16

	BYTE buff[100] = {0};
	DWORD n = 10;
	DWORD dwBytesRead;
	while (!done) {
		if(!ReadFile(device, &buff, n, &dwBytesRead, NULL)){
			printf("Error Reading from serial port.\n");
		} else {
			if (dwBytesRead > 0) {
				int stateVal = atoi((char*)buff);
				state.forward = stateVal & JOY_FWD;
				state.back    = stateVal & JOY_BACK;
				state.left    = stateVal & JOY_LEFT;
				state.right   = stateVal & JOY_RIGHT;
				state.fire    = stateVal & JOY_FIRE;
			}
		}
	}
	return 0; // TODO. error state

	// clean up function internal defines
#undef JOY_FWD
#undef JOY_BACK
#undef JOY_LEFT
#undef JOY_RIGHT
#undef JOY_FIRE
}

/**
 * Initialize the updater thread, which reads periodically the state of the accelerometer.
 * This method is called each times an Accelerometer object is created.
 */
void JoystickCompetitionPro::initUpdateThread() {
#if (defined(UNICODE) || defined (_UNICODE))
#define JOY_THREAD_MUTEX_NAME L"Global\\joyMtx_213078946"
#else
#define JOY_THREAD_MUTEX_NAME "Global\\joyMtx_213078946"
#endif

	bool err = false;
	if(mtx == NULL) {
		mtx = CreateMutexEx(NULL, JOY_THREAD_MUTEX_NAME, 0, MUTEX_ALL_ACCESS);
		if (mtx == NULL) {
			printf("CreateMutex error: %d\n", GetLastError());
			connected = false; // error: no connection
			return;
		}
	}

    DWORD dwWaitResult = WaitForSingleObject(mtx, INFINITE);
    switch (dwWaitResult) {
        // The thread got ownership of the mutex
        case WAIT_OBJECT_0:
			if (objectCounter(0) < 1) {
				device = CreateFile(JOY_DEVICE, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
				if(device==INVALID_HANDLE_VALUE){
					if(GetLastError()==ERROR_FILE_NOT_FOUND){
						printf("Error: Serial Port doesn't exist.\n");
						err = true;
					}
					printf("Error: Opening Serial Port.\n");
					err = true;
				}
				// Do some basic settings
				DCB serialParams = { 0 };
				serialParams.DCBlength = sizeof(serialParams);

				if (!GetCommState(device, &serialParams)) {
					printf("GetCommState failed.\n");
					err = true;
				}
				serialParams.BaudRate = CBR_57600;//CBR_9600;
				serialParams.ByteSize = 8;
				serialParams.StopBits = ONESTOPBIT;
				serialParams.fDtrControl = DTR_CONTROL_DISABLE;
				serialParams.fBinary = TRUE;
				serialParams.fOutxCtsFlow = FALSE;
				serialParams.fOutxDsrFlow = FALSE;
				serialParams.fDsrSensitivity = FALSE;
				serialParams.fOutX = FALSE;
				serialParams.fInX = FALSE;
				serialParams.fNull = TRUE;
				serialParams.fParity = FALSE;
				serialParams.Parity = NOPARITY;
				serialParams.EofChar = '\n';
				if (!SetCommState(device, &serialParams)) {
					printf("SetCommState failed.\n");
					err = true;
				}

				// Set timeouts
				COMMTIMEOUTS timeout = { 0 };
				timeout.ReadIntervalTimeout = 50;
				timeout.ReadTotalTimeoutConstant = 50;
				timeout.ReadTotalTimeoutMultiplier = 50;
				timeout.WriteTotalTimeoutConstant = 50;
				timeout.WriteTotalTimeoutMultiplier = 10;

				if (!SetCommTimeouts(device, &timeout)) {
					printf("SetCommTimeouts failed.\n");
					err = true;
				}

				FlushFileBuffers(device);

				updaterThread = CreateThread(NULL, 0, updateValues, NULL, 0, &dwThreadId);

		        // Check the return value for success.
		        if (updaterThread == NULL) {
		        	printf("CreateThread error: %d\n", GetLastError());
		        	connected = false; // thread failed --> no connection
		        	return;
		        }
			}
			objectCounter(1);
            if (! ReleaseMutex(mtx)) {
            	printf("ReleaseMutex error: %d\n", GetLastError());
            	err = true;
            }
            break;

        // The thread got ownership of an abandoned mutex
        case WAIT_ABANDONED:
        	connected = false; // abendoned mutex --> connection failed
            return;
    }
    if (!err) {
    	connected = true;
    }
#undef JOY_THREAD_MUTEX_NAME
}

/**
 * Close the updater thread if the last instance of Accelerometer is destroyed.
 * This method is called by the destructor of the Accelerometer class.
 */
void JoystickCompetitionPro::closeUpdateThread() {
    DWORD dwWaitResult = WaitForSingleObject(mtx, INFINITE);
    switch (dwWaitResult) {
        // The thread got ownership of the mutex
        // The thread got ownership of an abandoned mutex
        case WAIT_OBJECT_0:
        case WAIT_ABANDONED:
			if(objectCounter(-1) < 1) {
				done = true;
				connected = false;
			    WaitForSingleObject(updaterThread, INFINITE);
		        CloseHandle(updaterThread);
		        CloseHandle(device);
			}
			if (dwWaitResult != WAIT_ABANDONED) {
				if (! ReleaseMutex(mtx)) {
					printf("ReleaseMutex error: %d\n", GetLastError());
				}
			}
			if(objectCounter(-1) < 1) {
		        CloseHandle(mtx);
			}
            break;
    }
}

/**
 * Count the number of open joystick objects
 * @param count 1 count up, -1 count down or 0 to retrieve without change
 * @return the counter after change
 */
int JoystickCompetitionPro::objectCounter(int count) {
	static int openAccelerometers = 0;
	openAccelerometers += count;
	return openAccelerometers;
}
