/*
 * HapkitController.cpp
 *
 *  Created on: 08. Juni 2016
 *      Author: root
 */

#include <iostream>
#include <cstdlib>
#include <tchar.h>
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
DWORD HapkitController::timer = GetTickCount();

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

double HapkitController::steeringAmount(bool ignoreDirection) {
	long sign = state.paddlePos < 0 ? -1 : 1;
	double value = state.paddlePos * sign - HAPKIT_CENTER_DEADZONE;
	if (value < 0) value = 0;
	if (state.right) return 1;
	if (state.left) return (ignoreDirection ? 1 : -1);
	return (ignoreDirection ? 1 : sign) * 1.5 * value / (HAPKIT_PADDLE_MAX_STEERING - HAPKIT_CENTER_DEADZONE);
}

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
		if (!isFbTimeout()) {
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

			if (fbHitCar) {
				strncpy((char*)buff+n, "HitCar\n", 7);
				n += 7;
				fbHitCar = false;
			}
			if (fbCreak) {
				strncpy((char*)buff+n, "Creak\n", 6);
				n += 6;
				fbCreak = false;
			}
			if (fbSmash) {
				strncpy((char*)buff+n, "Smash\n", 6);
				n += 6;
				fbSmash = false;
			}
			if (fbWreck) {
				strncpy((char*)buff+n, "Wreck\n", 6);
				n += 6;
				fbWreck = false;
			}
			if (fbOffroad) {
				strncpy((char*)buff+n, "Offroad\n", 8);
				n += 8;
				fbOffroad = false;
			}
			if (fbGrounded) {
				strncpy((char*)buff+n, "Grounded\n", 9);
				n += 9;
				fbGrounded = false;
			}
			buff[n+1] = '\0';
		} else {// generate default value if game is not running
			strncpy((char*)buff, "FB 0 0\n", 8);
			n = 7;
		}
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
#define HAPKIT_THREAD_MUTEX_NAME TEXT("Global\\hapkitMtx_0923857129789")

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
				bool isDevPresent;
				TCHAR devPath[100];
				// search for the hapkit, if not found search for alternative device
				isDevPresent = getFTDIComPort(devPath, 100);
				if (!isDevPresent) isDevPresent = getDevicePath(ALTERNATIVE_DEVICE_ID, devPath, 100);
				if (isDevPresent) {
				device = CreateFile(devPath, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
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
				} else {
#if defined(DEBUG) || defined(_DEBUG) || defined(_DEBUG_CONSOLE)
					fprintf(out, "Error: Device not found.\n");
#endif
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
				serialParams.BaudRate = CBR_57600;
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
	resetFbTimer();
	fbCentrifugalAcc = acceleration;
#if defined(DEBUG) || defined(_DEBUG) || defined(_DEBUG_CONSOLE)
	fprintf(out, "Centrifugal Acceleration %d\n", acceleration);
#endif
}

void HapkitController::feedbackSteeringAngle(long angle) {
	resetFbTimer();
	fbSteeringAngle = angle;
#if defined(DEBUG) || defined(_DEBUG) || defined(_DEBUG_CONSOLE)
	fprintf(out, "Steering angle %.1f°\n", (double)angle/10);
#endif
}

void HapkitController::feedbackHitCar() {
	resetFbTimer();
	fbHitCar = true;
#if defined(DEBUG) || defined(_DEBUG) || defined(_DEBUG_CONSOLE)
	fprintf(out, "Hit Car\n");
#endif
}

void HapkitController::feedbackCreak() {
	resetFbTimer();
	fbCreak = true;
#if defined(DEBUG) || defined(_DEBUG) || defined(_DEBUG_CONSOLE)
	fprintf(out, "Creak\n");
#endif
}

void HapkitController::feedbackSmash() {
	resetFbTimer();
	fbSmash = true;
#if defined(DEBUG) || defined(_DEBUG) || defined(_DEBUG_CONSOLE)
	fprintf(out, "Smash\n");
#endif
}

void HapkitController::feedbackGrounded() {
	resetFbTimer();
	fbGrounded = true;
#if defined(DEBUG) || defined(_DEBUG) || defined(_DEBUG_CONSOLE)
	fprintf(out, "Grounded\n");
#endif
}

void HapkitController::feedbackWreck() {
	resetFbTimer();
	fbWreck = true;
#if defined(DEBUG) || defined(_DEBUG) || defined(_DEBUG_CONSOLE)
	fprintf(out, "Wreck\n");
#endif
}

void HapkitController::feedbackOffroad() {
	resetFbTimer();
	fbOffroad = true;
#if defined(DEBUG) || defined(_DEBUG) || defined(_DEBUG_CONSOLE)
	fprintf(out, "Offroad\n");
#endif
}

void HapkitController::resetFbTimer() {
	timer = GetTickCount();
}

bool HapkitController::isFbTimeout() {
	return (GetTickCount() - timer > HAPKIT_FEEDBACK_TIMEOUT) ? true : false;
}

bool HapkitController::getDevicePath(const TCHAR* deviceId, TCHAR* devicePath, size_t pathLength) {
	bool found = false;
	HDEVINFO                         hDevInfo;
	SP_DEVICE_INTERFACE_DATA         DevIntfData;
	PSP_DEVICE_INTERFACE_DETAIL_DATA DevIntfDetailData;
	SP_DEVINFO_DATA                  DevData;

	DWORD dwSize, dwType, dwMemberIdx;
	HKEY hKey;
	BYTE lpData[1024];

	// We will try to get device information set for all USB devices that have a
	// device interface and are currently present on the system (plugged in).
	hDevInfo = SetupDiGetClassDevs(
		&GUID_DEVINTERFACE_USB_DEVICE, NULL, 0, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);

	if (hDevInfo != INVALID_HANDLE_VALUE) {
		// Prepare to enumerate all device interfaces for the device information
		// set that we retrieved with SetupDiGetClassDevs(..)
		DevIntfData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
		dwMemberIdx = 0;

		// Next, we will keep calling this SetupDiEnumDeviceInterfaces(..) until this
		// function causes GetLastError() to return  ERROR_NO_MORE_ITEMS or the device
		// has been found. With each call the dwMemberIdx value needs to be incremented
		// to retrieve the next device interface information.

		SetupDiEnumDeviceInterfaces(hDevInfo, NULL, &GUID_DEVINTERFACE_USB_DEVICE,
			dwMemberIdx, &DevIntfData);

		while(GetLastError() != ERROR_NO_MORE_ITEMS && ! found) {
			// As a last step we will need to get some more details for each
			// of device interface information we are able to retrieve. This
			// device interface detail gives us the information we need to identify
			// the device (VID/PID), and decide if it's useful to us. It will also
			// provide a DEVINFO_DATA structure which we can use to know the serial
			// port name for a virtual com port.

			DevData.cbSize = sizeof(DevData);

			// Get the required buffer size. Call SetupDiGetDeviceInterfaceDetail with
			// a NULL DevIntfDetailData pointer, a DevIntfDetailDataSize
			// of zero, and a valid RequiredSize variable. In response to such a call,
			// this function returns the required buffer size at dwSize.

			SetupDiGetDeviceInterfaceDetail(
				  hDevInfo, &DevIntfData, NULL, 0, &dwSize, NULL);

			// Allocate memory for the DeviceInterfaceDetail struct. Don't forget to
			// deallocate it later!
			DevIntfDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dwSize);
			DevIntfDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

			if (SetupDiGetDeviceInterfaceDetail(hDevInfo, &DevIntfData,
				DevIntfDetailData, dwSize, &dwSize, &DevData)) {
				// Finally we can start checking if we've found a usable device,
				// by inspecting the DevIntfDetailData->DevicePath variable.

				if (NULL != _tcsstr(DevIntfDetailData->DevicePath, deviceId)) {
					found = true;
					// Copy only the portion of the devicePath, that fits into the buffer.
					// Thus, opening the device will fail if the buffer is too small, but
					// it won't cause a memory leak.
					size_t len = lstrlen((TCHAR*)DevIntfDetailData->DevicePath)+1;
					len = pathLength < len ? pathLength : len;
					lstrcpyn(devicePath, (TCHAR*)DevIntfDetailData->DevicePath, len);
					devicePath[len] = '\0';
				}
			}
			// cleaning up allocated memory
			HeapFree(GetProcessHeap(), 0UL, DevIntfDetailData);

			// Continue looping
			SetupDiEnumDeviceInterfaces(
				hDevInfo, NULL, &GUID_DEVINTERFACE_USB_DEVICE, ++dwMemberIdx, &DevIntfData);
		}
		SetupDiDestroyDeviceInfoList(hDevInfo);
	}
	return found;
}

bool HapkitController::getFTDIComPort(TCHAR* devicePath, size_t pathLength) {
	FT_HANDLE fthandle;
	FT_STATUS res;
	long portNumber;
/* **********************************************************************
// Source: FTDI example code
// Find the com port that has been assigned to your device.
/* **********************************************************************/

	res = FT_Open(0, &fthandle);

	if(res != FT_OK){
		return false;
	}

	res = FT_GetComPortNumber(fthandle,&portNumber);

	if(res != FT_OK){
		return false;
	}

	FT_Close(fthandle);
	if (portNumber < 1) {
		return false;
	}
	lstrcpyn(devicePath, TEXT("\\\\.\\COM14"), pathLength > 15 ? pathLength : 15);
	size_t n = 7;
	int i;
	TCHAR valString[10];
	for (i = 0; i < 10 && portNumber > 0 && n < pathLength ; ++i) {
        int rem = portNumber % 10;
        valString[i] = rem + _T('0');
        portNumber = portNumber/10;
	}
	for (i -= 1; i >= 0; --i) {
		devicePath[n++] = valString[i];
	}
	devicePath[n] = '\0';
	return true;
}
