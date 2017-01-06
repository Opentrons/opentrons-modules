/*
 * Code for an Arduino Uno to control the stepper motor
 * on the Magnetic-Bead Wash Station prototype
 * 
 * This version listens for a pin (controlPin) to be held LOW
 * then the magnets will move up
 * 
 * This is for OT.One version machines, which do not have
 * the ability to communicate with modules
 * 
*/




// GOES WITH PCB <MAGBEAD_V3>

int controlPin = 3;                 // pin received from Smoothie
int upSwitchPin = 9;                // the upper homing switch
int downSwitchPin = 8;              // the upper homing switch

#include <Stepper.h>
 
#define STEPS 200
Stepper stepper(STEPS, 4, 5, 6, 7);
int engageSwitch = A1;              // power ON/OFF the motor

int ledMovePin = A0;

int stepRange = STEPS * 20;
int currentStep;
boolean isUp = false;
 
void setup() {
  Serial.begin(9600);
  Serial.println("started");
  stepper.setSpeed(240);

  // setup limit switches
  pinMode(upSwitchPin,INPUT_PULLUP);
  pinMode(downSwitchPin,INPUT_PULLUP);

  // pin we listen to, from Smoothie
  pinMode(controlPin,INPUT_PULLUP);

  // optional LED will light up while moving
  pinMode(ledMovePin,OUTPUT);
  digitalWrite(ledMovePin,LOW);

  // motors driver's engage/disengage pin
  pinMode(engageSwitch,OUTPUT);
  digitalWrite(engageSwitch,LOW);

  // move up to find the pin
  currentStep = -stepRange;
  moveUp();
  // then move down to a known state
  moveDown();
}
 
void loop() {
  if(!digitalRead(controlPin) && !isUp){
    isUp = true;
    moveUp();
  }
  else if(digitalRead(controlPin) && isUp){
    isUp = false;
    moveDown();
  }
}

void moveUp() {
  digitalWrite(engageSwitch,HIGH);
  int stepLimit = stepRange/3;
  while(digitalRead(upSwitchPin) && currentStep<stepLimit){
    movePlatform(1);
  }
  Serial.println("found upper limit");
  currentStep = 0; // overwrite the currentStep so it's "homed"
  digitalWrite(engageSwitch,LOW);
}

void moveDown() {
  digitalWrite(engageSwitch,HIGH);
  while(digitalRead(downSwitchPin) && currentStep > -stepRange){
    movePlatform(-1);
  }
  Serial.println("found lower limit");
  digitalWrite(engageSwitch,LOW);
}

void movePlatform(int steps) {
  digitalWrite(ledMovePin,HIGH);
  currentStep += steps;
  stepper.step(steps);
  digitalWrite(ledMovePin,LOW);
}

