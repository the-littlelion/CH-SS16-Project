/*
 * HapkitController.cpp
 *
 *  Created on: 08. Juni 2016
 *      Author: root
 */

#include <iostream>
#include <cstdlib>
#include "HapkitController.h"

#if defined(DEBUG) || defined(_DEBUG)
extern FILE *out;
#elif defined(_DEBUG_CONSOLE)
FILE* out = stderr;
#endif

// initialize data
DWORD HapkitController::dwThreadId;
HANDLE HapkitController::updaterThread;
HANDLE HapkitController::mtx;
HANDLE HapkitController::device;
bool HapkitController::done = false;;

bool HapkitController::connected = false;
hapkitState HapkitController::state = {0};

long HapkitController::fbCentrifugalAcc = 0;
long HapkitController::fbSteeringAngle = 0;
bool HapkitController::fbHitCar = false;
bool HapkitController::fbCreak = false;
bool HapkitController::fbSmash = false;
bool HapkitController::fbWreck = false;
bool HapkitController::fbOffroad = false;
bool HapkitController::fbGrounded = false;

HapkitController::HapkitController() {
	initUpdateThread();
}

HapkitController::~HapkitController() {
	closeUpdateThread();
}

bool HapkitController::isConnected() {
	return connected;
}
hapkitState HapkitController::getState() {
	return state;
}

double force;//XXX debug info
char* pstr;//XXX debug info
DWORD WINAPI HapkitController::updateValues(void*) {
	// bitmasks for the joystate structure
#define BUTTON_FWD    1
#define BUTTON_BACK   2
#define BUTTON_LEFT   4
#define BUTTON_RIGHT  8
#define BUTTON_FIRE  16

	BYTE buff[100] = {0};
	DWORD n;
	DWORD dwBytes;
	while (!done) {
		// read states
		n = 0; // max length of string to read
		do {
			if(!ReadFile(device, &buff[n], 1, &dwBytes, NULL)){
				dwBytes = 0; // go sure to exit while if reading fails
#if defined(DEBUG) || defined(_DEBUG) || defined(_DEBUG_CONSOLE)
				fprintf(out, "Error Reading from serial port.\n");
#endif
			}
		} while (dwBytes == 1 && n < 99 && buff[n++] != '\n');
		buff[n-1] = '\0';
		if (n > 0 && dwBytes > 0) {
			char* ptr = (char*)buff; // pointer for strtok
			if ((ptr = strtok(ptr, " ")) != NULL) {
				int stateVal = atoi(ptr);
				state.forward = stateVal & BUTTON_FWD;
				state.back    = stateVal & BUTTON_BACK;
				state.left    = stateVal & BUTTON_LEFT;
				state.right   = stateVal & BUTTON_RIGHT;
				state.fire    = stateVal & BUTTON_FIRE;
			}
			if ((ptr = strtok(NULL, " ")) != NULL) {
				state.paddlePos = atof(ptr);
			}
		}
		// write feedback
		n = 0; // total length to write

		// output (space separated values): 'Feedback <centrifugalAcc> <steeringAngle>\n'
		char valString[20];
		strncpy((char*)buff+n, "FB ", 3);
		n += 3;
		_itoa(fbCentrifugalAcc, valString, 10);
		strncpy((char*)buff+n, valString, strlen(valString));
		n += strlen(valString) + 1;
		strncpy((char*)buff+n-1, " ", 1);
		_itoa(fbSteeringAngle, valString, 10);
		strncpy((char*)buff+n, valString, strlen(valString));
		n += strlen(valString) + 1;
		buff[n-1] = '\n';
		force = fbCentrifugalAcc;//XXX debug info
//		fbCentrifugalAcc = 0;//FIXME not implemented properly
//		fbSteeringAngle = 0;//FIXME not implemented properly

		if (fbHitCar) {//FIXME refactoring needed
			strncpy((char*)buff+n, "HitCar\n", 7);
			n += 7;
			fbHitCar = false;
		}
		if (fbCreak) {//FIXME refactoring needed
			strncpy((char*)buff+n, "Creak\n", 6);
			n += 6;
			fbCreak = false;
		}
		if (fbSmash) {//FIXME refactoring needed
			strncpy((char*)buff+n, "Smash\n", 6);
			n += 6;
			fbSmash = false;
		}
		if (fbWreck) {//FIXME refactoring needed
			strncpy((char*)buff+n, "Wreck\n", 6);
			n += 6;
			fbWreck = false;
		}
		if (fbOffroad) {//FIXME refactoring needed
			strncpy((char*)buff+n, "Offroad\n", 8);
			n += 8;
			fbOffroad = false;
		}
		if (fbGrounded) {//FIXME refactoring needed
			strncpy((char*)buff+n, "Grounded\n", 9);
			n += 9;
			fbGrounded = false;
		}
		buff[n+1] = '\0';
		if(!WriteFile(device, &buff, n, &dwBytes, NULL) || dwBytes <= 0) {
#if defined(DEBUG) || defined(_DEBUG) || defined(_DEBUG_CONSOLE)
			fprintf(out, "Error Writing to serial port.\n");
#endif
		}
	}
	return 0; // TODO. error state

	// clean up function internal defines
#undef BUTTON_FWD
#undef BUTTON_BACK
#undef BUTTON_LEFT
#undef BUTTON_RIGHT
#undef BUTTON_FIRE
}

/**
 * Initialize the updater thread, which reads periodically the state of the device.
 * This method is called each times an object instance is created.
 */
void HapkitController::initUpdateThread() {
#if (defined(UNICODE) || defined (_UNICODE))
#define HAPKIT_THREAD_MUTEX_NAME L"Global\\hapkitMtx_0923857129789"
#else
#define HAPKIT_THREAD_MUTEX_NAME "Global\\hapkitMtx_0923857129789"
#endif

	bool err = false;
	if(mtx == NULL) {
		mtx = CreateMutexEx(NULL, HAPKIT_THREAD_MUTEX_NAME, 0, MUTEX_ALL_ACCESS);
		if (mtx == NULL) {
#if defined(DEBUG) || defined(_DEBUG) || defined(_DEBUG_CONSOLE)
			fprintf(out, "CreateMutex error: %d\n", GetLastError());
#endif
			connected = false; // error: no connection
			return;
		}
	}

    DWORD dwWaitResult = WaitForSingleObject(mtx, INFINITE);
    switch (dwWaitResult) {
        // The thread got ownership of the mutex
        case WAIT_OBJECT_0:
			if (objectCounter(0) < 1) {
				device = CreateFile(SERIAL_DEVICE, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
				if(device==INVALID_HANDLE_VALUE){
					if(GetLastError()==ERROR_FILE_NOT_FOUND){
#if defined(DEBUG) || defined(_DEBUG) || defined(_DEBUG_CONSOLE)
						fprintf(out, "Error: Serial Port doesn't exist.\n");
#endif
						err = true;
					}
#if defined(DEBUG) || defined(_DEBUG) || defined(_DEBUG_CONSOLE)
					fprintf(out, "Error: Opening Serial Port.\n");
#endif
					err = true;
				}
				// Do some basic settings
				DCB serialParams = { 0 };
				serialParams.DCBlength = sizeof(serialParams);

				if (!GetCommState(device, &serialParams)) {
#if defined(DEBUG) || defined(_DEBUG) || defined(_DEBUG_CONSOLE)
					fprintf(out, "GetCommState failed.\n");
#endif
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
#if defined(DEBUG) || defined(_DEBUG) || defined(_DEBUG_CONSOLE)
					fprintf(out, "SetCommState failed.\n");
#endif
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
#if defined(DEBUG) || defined(_DEBUG) || defined(_DEBUG_CONSOLE)
					fprintf(out, "SetCommTimeouts failed.\n");
#endif
					err = true;
				}

				FlushFileBuffers(device);

				updaterThread = CreateThread(NULL, 0, updateValues, NULL, 0, &dwThreadId);

		        // Check the return value for success.
		        if (updaterThread == NULL) {
#if defined(DEBUG) || defined(_DEBUG) || defined(_DEBUG_CONSOLE)
		        	fprintf(out, "CreateThread error: %d\n", GetLastError());
#endif
		        	connected = false; // thread failed --> no connection
		        	return;
		        }
			}
			objectCounter(1);
            if (! ReleaseMutex(mtx)) {
#if defined(DEBUG) || defined(_DEBUG) || defined(_DEBUG_CONSOLE)
            	fprintf(out, "ReleaseMutex error: %d\n", GetLastError());
#endif
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
#undef HAPKIT_THREAD_MUTEX_NAME
}

/**
 * Close the updater thread if the last instance is destroyed.
 * This method is called by the destructor of the HapkitController class.
 */
void HapkitController::closeUpdateThread() {
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
#if defined(DEBUG) || defined(_DEBUG) || defined(_DEBUG_CONSOLE)
					fprintf(out, "ReleaseMutex error: %d\n", GetLastError());
#endif
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
int HapkitController::objectCounter(int count) {
	static int openInstances = 0;
	openInstances += count;
	return openInstances;
}

void HapkitController::feedbackCentrifugalAccel(long acceleration) {
	//FIXME timeout
	fbCentrifugalAcc = acceleration;
#if defined(DEBUG) || defined(_DEBUG) || defined(_DEBUG_CONSOLE)
	fprintf(out, "Centrifugal Acceleration %d\n", acceleration);
#endif
}

void HapkitController::feedbackSteeringAngle(long angle) {
	//FIXME timeout
	fbSteeringAngle = angle;
#if defined(DEBUG) || defined(_DEBUG) || defined(_DEBUG_CONSOLE)
	fprintf(out, "Steering angle %.1f°\n", (double)angle/10);
#endif
}

void HapkitController::feedbackHitCar() {
	fbHitCar = true;
#if defined(DEBUG) || defined(_DEBUG) || defined(_DEBUG_CONSOLE)
	fprintf(out, "Hit Car\n");
#endif
}

void HapkitController::feedbackCreak() {
	fbCreak = true;
#if defined(DEBUG) || defined(_DEBUG) || defined(_DEBUG_CONSOLE)
	fprintf(out, "Creak\n");
#endif
}

void HapkitController::feedbackSmash() {
	fbSmash = true;
#if defined(DEBUG) || defined(_DEBUG) || defined(_DEBUG_CONSOLE)
	fprintf(out, "Smash\n");
#endif
}

void HapkitController::feedbackGrounded() {
	fbGrounded = true;
#if defined(DEBUG) || defined(_DEBUG) || defined(_DEBUG_CONSOLE)
	fprintf(out, "Grounded\n");
#endif
}

void HapkitController::feedbackWreck() {
	fbWreck = true;
#if defined(DEBUG) || defined(_DEBUG) || defined(_DEBUG_CONSOLE)
	fprintf(out, "Wreck\n");
#endif
}

void HapkitController::feedbackOffroad() {
	fbOffroad = true;
#if defined(DEBUG) || defined(_DEBUG) || defined(_DEBUG_CONSOLE)
	fprintf(out, "Offroad\n");
#endif
}
