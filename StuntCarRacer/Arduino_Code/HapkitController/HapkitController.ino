/*
 * HapkitController.ino
 *
 *  Created on: 08. Juni 2016
 *      Author: luggi, stefha
 */
#include <math.h>

/*
    DECLARATION
*/
#define BUFSIZE 100        // size of receive buffer
char serialBuffer[BUFSIZE];

// Pin
int pwmPin = 5; // PWM output pin for motor PD5 OC0B
int dirPin = 8; // direction output pin for motor
int sensorPosPin = A2; // input pin for MR sensor
// inputs for the joystick
int joyFwdPin = 9;
int joyBackPin = 10;
int joyLeftPin = 11;
int joyRightPin = 12;
int joyFirePin = 13;
// outout for LED - feedback tracking
int feedbackLEDPin = 7;

// position tracking

int updatedPos = 0;     // keeps track of the latest updated value of the MR sensor reading
int rawPos = 0;         // current raw reading from MR sensor
int lastRawPos = 0;     // last raw reading from MR sensor
int lastLastRawPos = 0; // last last raw reading from MR sensor
int flipNumber = 0;     // keeps track of the number of flips over the 180deg mark
int tempOffset = 0;
int rawDiff = 0;
int lastRawDiff = 0;
int rawOffset = 0;
int lastRawOffset = 0;
const int flipThresh = 700;  // threshold to determine whether or not a flip over the 180 degree mark occurred
boolean flipped = false;


// Kinematics
double xh = 0;         // position of the handle [m]
double lastXh = 0;     //last x position of the handle
double vh = 0;         //velocity of the handle
double lastVh = 0;     //last velocity of the handle
double lastLastVh = 0; //last last velocity of the handle
double rp = 0.004191;   //[m]
double rs = 0.073152;   //[m]
double rh = 0.065659;   //[m]
// Force output variables
double force = 0;           // force at the handle
double force_vibrate = 0;
double Tp = 0;              // torque of the motor pulley
double duty = 0;            // duty cylce (between 0 and 255)
unsigned int output = 0;    // output command to the motor
float set_position = 0.0f;

// custom vars
uint32_t nextcall = 0;
const uint16_t intervalperiod = 50*64; // 100 milliseconds
uint32_t nextcall2 = 1000;
float vibrationfrequency = 10.0f;
float amplitude = 1.0f;
bool on_off = true;
float springfactor = 0.0f;

/*
      Setup function - this function run once when reset button is pressed.
*/

void setup() {
  // Set up serial communication
  Serial.begin(57600);

  // Set PWM frequency
  setPwmFrequency(pwmPin, 1);
  // Input pins
  pinMode(sensorPosPin, INPUT); // set MR sensor pin to be an
  // set joystick pins as input with pullup 
  pinMode(joyFwdPin, INPUT_PULLUP);
  pinMode(joyBackPin, INPUT_PULLUP);
  pinMode(joyLeftPin, INPUT_PULLUP);
  pinMode(joyRightPin, INPUT_PULLUP);
  pinMode(joyFirePin, INPUT_PULLUP);

  // Output pins
  pinMode(pwmPin, OUTPUT);  // PWM pin for motor
  pinMode(dirPin, OUTPUT);  // dir pin for motor
  pinMode(feedbackLEDPin, OUTPUT); //XXX DEBUG INFO for feedback tracking

  // Initialize motor
  analogWrite(pwmPin, 0);     // set to not be spinning (0/255)
  digitalWrite(dirPin, LOW);  // set direction

  // Initialize position valiables
  lastLastRawPos = analogRead(sensorPosPin);
  lastRawPos = analogRead(sensorPosPin);
}

/*
   setPwmFrequency
*/
void setPwmFrequency(int pin, int divisor) {
  byte mode;
  if (pin == 5 || pin == 6 || pin == 9 || pin == 10) {
    switch (divisor) {
      case 1: mode = 0x01; break;
      case 8: mode = 0x02; break;
      case 64: mode = 0x03; break;
      case 256: mode = 0x04; break;
      case 1024: mode = 0x05; break;
      default: return;
    }
    if (pin == 5 || pin == 6) {
      TCCR0B = TCCR0B & 0b11111000 | mode;
    } else {
      TCCR1B = TCCR1B & 0b11111000 | mode;
    }
  } else if (pin == 3 || pin == 11) {
    switch (divisor) {
      case 1: mode = 0x01; break;
      case 8: mode = 0x02; break;
      case 32: mode = 0x03; break;
      case 64: mode = 0x04; break;
      case 128: mode = 0x05; break;
      case 256: mode = 0x06; break;
      case 1024: mode = 0x7; break;
      default: return;
    }
    TCCR2B = TCCR2B & 0b11111000 | mode;
  }
}

/*
      Output to motor
*/
void motorControl()
{
  if(abs(xh) > 35.0f)
    force = 0.0f;
  Tp = rp / rs * rh * (force + force_vibrate);  // Compute the require motor pulley torque (Tp) to generate that force
  // Determine correct direction for motor torque
  if ((force+force_vibrate) < 0) {
    digitalWrite(dirPin, HIGH);
  } else {
    digitalWrite(dirPin, LOW);
  }

  // Compute the duty cycle required to generate Tp (torque at the motor pulley)
  duty = sqrt(abs(Tp) / 0.03);

  // Make sure the duty cycle is between 0 and 100%
  if (duty > 1) {
    duty = 1;
  } else if (duty < 0) {
    duty = 0;
  }
  output = (int)(duty * 255);  // convert duty cycle to output signal
  analogWrite(pwmPin, output); // output the signal
}

/*
    readPosCount() function
*/
void readPosCount() {
  // Get voltage output by MR sensor
  rawPos = analogRead(sensorPosPin);  //current raw position from MR sensor
  // Calculate differences between subsequent MR sensor readings
  rawDiff = rawPos - lastRawPos;          //difference btwn current raw position and last raw position
  lastRawDiff = rawPos - lastLastRawPos;  //difference btwn current raw position and last last raw position
  rawOffset = abs(rawDiff);
  lastRawOffset = abs(lastRawDiff);

  // Update position record-keeping vairables
  lastLastRawPos = lastRawPos;
  lastRawPos = rawPos;

  // Keep track of flips over 180 degrees
  if ((lastRawOffset > flipThresh) && (!flipped)) { // enter this anytime the last offset is greater than the flip threshold AND it has not just flipped
    if (lastRawDiff > 0) {       // check to see which direction the drive wheel was turning
      flipNumber--;              // cw rotation
    } else {                     // if(rawDiff < 0)
      flipNumber++;              // ccw rotation
    }
    if (rawOffset > flipThresh) { // check to see if the data was good and the most current offset is above the threshold
      updatedPos = rawPos + flipNumber * rawOffset; // update the pos value to account for flips over 180deg using the most current offset
      tempOffset = rawOffset;
    } else {                     // in this case there was a blip in the data and we want to use lastactualOffset instead
      updatedPos = rawPos + flipNumber * lastRawOffset; // update the pos value to account for any flips over 180deg using the LAST offset
      tempOffset = lastRawOffset;
    }
    flipped = true;            // set boolean so that the next time through the loop won't trigger a flip
  } else {                        // anytime no flip has occurred
    updatedPos = rawPos + flipNumber * tempOffset; // need to update pos based on what most recent offset is
    flipped = false;
  }
    
  //Serial.println(updatedPos);
}


/*
    calPosMeter()
*/
void calPosMeter()
{
  double rh = 65.659;   //[mm]
  double ts = -.0107 * updatedPos + 7.9513; // Compute the angle of the sector pulley (ts) in degrees based on updatedPos
  xh = rh * (ts * 3.14159 / 180); // Compute the position of the handle based on ts
  vh = -(.95 * .95) * lastLastVh + 2 * .95 * lastVh + (1 - .95) * (1 - .95) * (xh - lastXh) / .0001; // filtered velocity (2nd-order filter)
  lastXh = xh;
  lastLastVh = lastVh;
  lastVh = vh;
  
//  Serial.println(vh);
}

// read joystick state
byte readJoystick() {
	byte result = 0;
	digitalRead(joyFwdPin) == LOW ? result += 1 : 0;
	digitalRead(joyBackPin) == LOW ? result += 2 : 0;
	//(xh>2.0f) == LOW ? result += 4 : 0;
	//(xh<-2.0f) == LOW ? result += 8 : 0;
	//digitalRead(joyFirePin) == LOW ? result += 16 : 0;
	return result;
}


/*
    forceRendering()
*/
void forceRendering(void) {
  static float last_time = 0.0f;
  static float last_error = 0.0f;
  static float I = 0.0f;

  const static float kP = 0.1f;
  static float kI = 0.0f;
  static float kD = 0.0f;//0.00004f;
  float kP_scaled;

  const float MAX_DIFFERENCE = 0.0f;
  const float LIMITPLUS = 1.0f;
  const float LIMITMINUS = -1.0f;
  const float MAX_FORCE_OUTPUT = 0.0f;
  float actualForceUsed = 0.0f;

  kP_scaled = kP * abs(springfactor) / 1000.0f;

  float error = set_position - xh;

  float P = error * kP_scaled;

  float D = -vh * kD;

  force = P + I + D;
}

void shake(int interval_ms,float frequency_hz, float strength)
{
    vibrationfrequency = frequency_hz;
    nextcall = millis() + (uint32_t)interval_ms * 64UL;
    amplitude = strength;
    on_off = true;
}

/*
    Loop function
*/
void loop() {
  // read the position in count
  readPosCount();
  calPosMeter();
  
  if(millis()>nextcall)
    on_off = false;
  forceRendering();

    
  if(millis() > nextcall2){
    nextcall2 += intervalperiod;
    Serial.print(readJoystick());
    Serial.print(" ");
    Serial.println(xh);
  }


  // calculations for haptic event feedback
  on_off ? force_vibrate = amplitude * sin(vibrationfrequency*2.0f*M_PI*millis()/64000.0f) : force_vibrate = 0;
  
  motorControl();
}

/*
    Read serial data if available.
	Called on a serial receiving event.
*/
void serialEvent() {
  String fbEvent = "";
  emptyArray(serialBuffer, BUFSIZE);

  // read and access command
  Serial.readBytesUntil('\n', serialBuffer, BUFSIZE);
  serialBuffer[BUFSIZE-1] = '\0'; // assert a terminated string
  fbEvent += serialBuffer;
  //TODO the reading and decoding goes here

  if (fbEvent.startsWith("FB")) {
	springfactor = atof(fbEvent.substring(3).c_str()); // roughly from 0 to 1000
    //Serial.print("force: ");
    //Serial.println(force);
  } else if (fbEvent.startsWith("HitCar")) {
	shake(1000, 100.0f, 0.5f);
  } else if (fbEvent.startsWith("Creak")) {
    shake(500, 100.0f, 0.4f);
  } else if (fbEvent.startsWith("Smash")) {
	shake(1200, 100.0f, 1.5f);
  } else if (fbEvent.startsWith("Wreck")) {
	shake(700, 200.0f, 0.8f);
  } else if (fbEvent.startsWith("Offroad")) {
	shake(1000, 20.0f, 0.5f);
  } else if (fbEvent.startsWith("Grounded")) {
	shake(300, 10.0f, 0.3f);
  }
}

// clear contents of a char array
void emptyArray(char* array, int length) {
  for (uint8_t i = 0; i < length; ++i) {
    *(array + i) = '\0';
  }
}
