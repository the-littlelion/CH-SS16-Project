/*
 * HapkitController.cpp
 *
 *  Created on: 08. Juni 2016
 *      Author: root
 */

#include <iostream>
#include "HapkitController.h"

#if defined(DEBUG) || defined(_DEBUG)
extern FILE *out;
#endif

DWORD HapkitController::dwThreadId;
HANDLE HapkitController::updaterThread;
HANDLE HapkitController::mtx;
HANDLE HapkitController::device;
bool HapkitController::done = false;;

bool HapkitController::connected = false;
hapkitState HapkitController::state = {0};

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

DWORD WINAPI HapkitController::updateValues(void*) {
	// bitmasks for the joystate structure
#define BUTTON_FWD    1
#define BUTTON_BACK   2
#define BUTTON_LEFT   4
#define BUTTON_RIGHT  8
#define BUTTON_FIRE  16

	BYTE buff[100] = {0};
	DWORD n = 10;
	DWORD dwBytesRead;
	while (!done) {
		if(!ReadFile(device, &buff, n, &dwBytesRead, NULL)){
#if defined(DEBUG) || defined(_DEBUG)
			fprintf(out, "Error Reading from serial port.\n");
#endif

		} else {
			if (dwBytesRead > 0) {
				int stateVal = atoi((char*)buff);
				state.forward = stateVal & BUTTON_FWD;
				state.back    = stateVal & BUTTON_BACK;
				state.left    = stateVal & BUTTON_LEFT;
				state.right   = stateVal & BUTTON_RIGHT;
				state.fire    = stateVal & BUTTON_FIRE;
			}
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
#if defined(DEBUG) || defined(_DEBUG)
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
#if defined(DEBUG) || defined(_DEBUG)
						fprintf(out, "Error: Serial Port doesn't exist.\n");
#endif
						err = true;
					}
#if defined(DEBUG) || defined(_DEBUG)
					fprintf(out, "Error: Opening Serial Port.\n");
#endif
					err = true;
				}
				// Do some basic settings
				DCB serialParams = { 0 };
				serialParams.DCBlength = sizeof(serialParams);

				if (!GetCommState(device, &serialParams)) {
#if defined(DEBUG) || defined(_DEBUG)
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
#if defined(DEBUG) || defined(_DEBUG)
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
#if defined(DEBUG) || defined(_DEBUG)
					fprintf(out, "SetCommTimeouts failed.\n");
#endif
					err = true;
				}

				FlushFileBuffers(device);

				updaterThread = CreateThread(NULL, 0, updateValues, NULL, 0, &dwThreadId);

		        // Check the return value for success.
		        if (updaterThread == NULL) {
#if defined(DEBUG) || defined(_DEBUG)
		        	fprintf(out, "CreateThread error: %d\n", GetLastError());
#endif
		        	connected = false; // thread failed --> no connection
		        	return;
		        }
			}
			objectCounter(1);
            if (! ReleaseMutex(mtx)) {
#if defined(DEBUG) || defined(_DEBUG)
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
#if defined(DEBUG) || defined(_DEBUG)
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
#if defined(DEBUG) || defined(_DEBUG)
#endif
	fprintf(stderr, "Centrifugal Acceleration %d\n", acceleration);//FIXME DEBUG
}

void HapkitController::feedbackHitCar() {
#if defined(DEBUG) || defined(_DEBUG)
#endif
	fprintf(stderr, "Hit Car\n");//FIXME DEBUG
}

void HapkitController::feedbackCreak() {
#if defined(DEBUG) || defined(_DEBUG)
#endif
	fprintf(stderr, "Creak\n");//FIXME DEBUG
}

void HapkitController::feedbackSmash() {
#if defined(DEBUG) || defined(_DEBUG)
#endif
	fprintf(stderr, "Smash\n");//FIXME DEBUG
}

void HapkitController::feedbackGrounded() {
#if defined(DEBUG) || defined(_DEBUG)
#endif
	fprintf(stderr, "Grounded\n");//FIXME DEBUG
}

void HapkitController::feedbackWreck() {
#if defined(DEBUG) || defined(_DEBUG)
#endif
	fprintf(stderr, "Wreck\n");//FIXME DEBUG
}

void HapkitController::feedbackOffroad() {
#if defined(DEBUG) || defined(_DEBUG)
#endif
	fprintf(stderr, "Offroad\n");//FIXME DEBUG
}
