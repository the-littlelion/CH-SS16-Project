/*
 * ArduinoAmigaJoystick.ino
 * Arduino code to read joystick states
 *
 *  Created on: 05. Juni 2016
 *      Author: theoneone
 */

/*
    DECLARATION
*/

// uncomment this define to enable amiga analog joystick support
#define USE_ANALOG_JOYSTICK

// Pin (for the hapkit - actually they are just set to a safe mode)
int pwmPin = 5; // PWM output pin for motor
int dirPin = 8; // direction output pin for motor
int sensorPosPin = A2; // input pin for MR sensor
// inputs for the joystick
int joyFwdPin = 9;
int joyBackPin = 10;
int joyLeftPin = 11;
int joyRightPin = 12;
int joyFirePin = 13;
int analogXPin = A0;
int analogYPin = A1;
int analogXCenter, analogYCenter; // store neutral position at startup

/*
      Setup function - this function run once when reset button is pressed.
*/

void setup() {
  // Set up serial communication
  Serial.begin(57600);

  // Input pins
  pinMode(sensorPosPin, INPUT); // set MR sensor pin to be an input
  // set joystick pins as input with pullup
  pinMode(joyFwdPin, INPUT_PULLUP);
  pinMode(joyBackPin, INPUT_PULLUP);
  pinMode(joyLeftPin, INPUT_PULLUP);
  pinMode(joyRightPin, INPUT_PULLUP);
  pinMode(joyFirePin, INPUT_PULLUP);

  // Output pins
  pinMode(pwmPin, OUTPUT);  // PWM pin for motor
  pinMode(dirPin, OUTPUT);  // dir pin for motor

  // Initialize motor
  analogWrite(pwmPin, 0);     // set to not be spinning (0/255)
  digitalWrite(dirPin, LOW);  // set direction

  // initialize analog joystick
#ifdef USE_ANALOG_JOYSTICK
  analogXCenter = analogRead(analogXPin);
  analogYCenter = analogRead(analogYPin);
#endif
}

// read joystick state
uint8_t readJoystick() {
	uint8_t result = 0;
	digitalRead(joyFwdPin) == LOW ? result += 1 : 0;
	digitalRead(joyBackPin) == LOW ? result += 2 : 0;
	digitalRead(joyLeftPin) == LOW ? result += 4 : 0;
	digitalRead(joyRightPin) == LOW ? result += 8 : 0;
	digitalRead(joyFirePin) == LOW ? result += 16 : 0;
	return result;
}

/*
    Loop function
*/
bool dir;
int32_t nextcall = 1000;
void loop() {

  if(millis() > nextcall) {
    nextcall += 50;

	Serial.print(readJoystick()); // return joystick state
#ifdef USE_ANALOG_JOYSTICK
    Serial.print(" ");
    Serial.print(analogRead(analogXPin)-analogXCenter);
    Serial.print(" ");
    Serial.print(analogRead(analogYPin)-analogYCenter);
#endif
    Serial.println();
  }
  
}
