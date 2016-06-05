#include <math.h>

/*
    DECLARATION
*/

// Pin
int pwmPin = 5; // PWM output pin for motor
int dirPin = 8; // direction output pin for motor
int sensorPosPin = A2; // input pin for MR sensor
// inputs for the joystick
int joyFwdPin = 9;
int joyBackPin = 10;
int joyLeftPin = 11;
int joyRightPin = 12;
int joyFirePin = 13;

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
double Tp = 0;              // torque of the motor pulley
double duty = 0;            // duty cylce (between 0 and 255)
unsigned int output = 0;    // output command to the motor


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

  // Initialize position valiables
  lastLastRawPos = analogRead(sensorPosPin);
  lastRawPos = analogRead(sensorPosPin);
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
  double ts = -.0107 * updatedPos + 4.9513; // Compute the angle of the sector pulley (ts) in degrees based on updatedPos
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
  // read the position in count
  readPosCount();
  calPosMeter();

  if(millis() > nextcall) {
    nextcall += 200;
    analogWrite(pwmPin, 100);
    dir = !dir;
    digitalWrite(dirPin, dir);
	
	Serial.println(readJoystick()); // return joystick state
  }
  
}
