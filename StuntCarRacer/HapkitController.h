/*
 * HapkitController.h
 *
 *  Created on: 08. Juni 2016
 *      Author: theoneone (Gerhard Aigner)
 */

#ifndef HAPKITCONTROLLER_H_
#define HAPKITCONTROLLER_H_

#include <Windows.h>
#include <setupapi.h>
#include <initguid.h>
#include "ftd2xx.h"

/**
 * The device ID's
 */
//#define HAPKIT_DEVICE_ID TEXT("vid_0403&pid_6001") /* the vendor- and device ID of the hapkit - unused because FTDI does it in it's own way */
#define ALTERNATIVE_DEVICE_ID TEXT("vid_2341&pid_0043") /* alternative device: Arduino UNO with joystick */

/**
 * Define the width of the dead zone between turning left and right.
 */
#define HAPKIT_PADDLE_MAX_STEERING 35 /* the steering amount in end position of the paddle */
#define HAPKIT_CENTER_DEADZONE (0.10 * (HAPKIT_PADDLE_MAX_STEERING)) /* deadband without steering if paddle in center position */
#define HAPKIT_FEEDBACK_TIMEOUT 2000 /* after the feedback hasn't been updated for this time in ms the feedback values are reset to default */
#define IF_HAPKIT_HAS_FIRE_BUTTON false /* change to 'true' if the hapkit comes with extra fire button */

/**
 * This is the GUID for the USB device class
 * (A5DCBF10-6530-11D2-901F-00C04FB951ED)
 */
DEFINE_GUID(GUID_DEVINTERFACE_USB_DEVICE, 0xA5DCBF10L, 0x6530, 0x11D2, 0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED);

/**
 * container for all state values received from the hapkit
 */
typedef struct _hapkitState {
	bool forward;
	bool back;
	bool left; // spare field for button
	bool right; // spare field for button
	bool fire;
	double paddlePos;
} hapkitState;

/**
 * Driver class for the hapkit as controller with force feedback.
 * Instances of this class share the same state. A single thread
 * gathers data from the COM-port and supplies them to all instances.
 * With creation of the first instance the thread is initialized
 * and will be terminated with the destruction of the last instance.
 *
 * Feedback data are sent immediately and should be handled by events
 * on the hapkit device.
 */
class HapkitController {
public:
	/**
	 * Create a new controller instance.
	 * All instances are connected to the same device.
	 * The initialization of the first instance starts
	 * a helper thread, which receives the state from
	 * the connected device.
	 */
	HapkitController();

	/**
	 * Destruction of an instance. With removing the last
	 * instance, the helper thread will be terminated.
	 */
	virtual ~HapkitController();

	/**
	 * Detect a connected device.
	 * @retval true if the hapkit is connected
	 * @retval false otherwise
	 */
	bool isConnected();

	/**
	 * Get the last update of the states of the hapkit.
	 * @return all button states and the paddle position.
	 */
	hapkitState getState();

	/**
	 * Calculate steering amount normalized between 0 and 150%.
	 * @param ignoreDirection if true return only positive values, otherwise use sign to discriminate left from right direction
	 * @return the steering amount
	 */
	double steeringAmount(bool ignoreDirection);

	/**
	 * Require a force feedback to the centrifugal force caused by driving through a curve.
	 * @param acceleration
	 */
	void feedbackCentrifugalAccel(long acceleration);

	/**
	 * Setpoint for angle from the center position.
	 * @param angle angle in tenths of degrees
	 */
	void feedbackSteeringAngle(long angle);

	/**
	 * Feedback request to a car to car collision event.
	 */
	void feedbackHitCar();

	/**
	 * Feedback caused by damage to the car (creak sound is played).
	 */
	void feedbackCreak();

	/**
	 * Feedback caused by strong collisions (e.g. by smashing against a wall,
	 * glass breaking sound is played if this happens).
	 */
	void feedbackSmash();

	/**
	 * Feedback caused by wrecking the car (scratching sound and sparkles).
	 * Note: the sparkles are not rendered, yet.
	 */
	void feedbackWreck();

	/**
	 * Feedback when the car falls off the road.
	 */
	void feedbackOffroad();

	/**
	 * Feedback if the car touches the road (e.g. after releasing off the chains or after a jump).
	 */
	void feedbackGrounded();

private:
    static DWORD dwThreadId;       ///< The ID of the helper thread.
    static HANDLE updaterThread;   ///< The file handle of the helper thread.
    static HANDLE mtx;             ///< Mutex to access the class in a thread safe mode.
	static HANDLE device;          ///< File handle of the COM-Port.
	static bool done;              ///< Termination flag for the helper thread.

	static bool connected;         ///< Device connection state.
	static hapkitState state;      ///< State information of the hapkit device
	static DWORD timer;            ///< Timeout for haptic feedback.

	static long fbLastUpdate;      ///< Time in ticks, the last continuously sent feedback data were updated
	static long fbCentrifugalAcc;  ///< Request of force-feedback (sign = direction, value = force)
	static long fbSteeringAngle;   ///< Angle of paddle in tenths of degrees from center position.
	static bool fbHitCar;          ///< Trigger Hit Car event
	static bool fbCreak;           ///< Trigger Creak event
	static bool fbSmash;           ///< Trigger Smash event
	static bool fbWreck;           ///< Trigger Wreck event
	static bool fbOffroad;         ///< Trigger Off Road event
	static bool fbGrounded;        ///< Trigger Grounded event

	/**
	 * Counter to hold the number of object instances.
	 *
	 * This method is used to check when the last instance is destroyed and
	 * the helper thread needs to be terminated.
	 * @param count +1 or -1 to count up or down, 0 to retrieve the number of initialized instances.
	 * @return the actual number of instances.
	 */
	static int objectCounter(int count);

	/**
	 * Update the state of all buttons and the paddle position.
	 *
	 * This method reads data from the COM-port. It is called periodically
	 * from the helper thread.
	 */
	static DWORD WINAPI updateValues(void*);

	/**
	 * Initialize the helper thread.
	 *
	 * This method initializes the helper thread when the first instance is
	 * created. It is called by the constructor. Subsequent calls maintain
	 * an internal counter which holds the actual number of instances.
	 */
	static void initUpdateThread();

	/**
	 * Terminate the helper thread if the last object instance is to be destroyed.
	 *
	 * This method maintains the object counter and if the last instance is
	 * deleted the helper thread is terminated and the COM-port is closed.
	 */
	static void closeUpdateThread();

	/**
	 * Reset the timer for haptic feedback.
	 */
	static void resetFbTimer();

	/**
	 * Timeout occured? If yes, don't send feedback data.
	 * @retval true if timeout
	 * @retval false otherwise
	 */
	static bool isFbTimeout();

	/**
	 * Retrieve the device path from VID and PID string.
	 * @param deviceId The identifying string. It should look like this example: "vid_2341&pid_0043".
	 * @param devicePath Pointer to the buffer for the device path.
	 * @param pathLength length of the buffer including the trailing '\0' character
	 * @retval true if the device has been found
	 * @retval false otherwise
	 */
	static bool getDevicePath(const TCHAR* deviceId, TCHAR* devicePath, size_t pathLength);

	/**
	 * Detect the FTDI device by the port number.
	 * @param devicePath Pointer to the buffer for the device path.
	 * @param pathLength length of the buffer including the trailing '\0' character
	 * @retval true if the device has been found
	 * @retval false otherwise
	 */
	static bool HapkitController::getFTDIComPort(TCHAR* devicePath, size_t pathLength);
};

#endif /* HAPKITCONTROLLER_H_ */
