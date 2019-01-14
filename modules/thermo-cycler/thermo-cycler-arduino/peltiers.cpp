#include "Peltiers.h"

Peltiers::Peltiers(){
}

void Peltiers::disable(){
//  Serial.println("Peltiers are both OFF");
  pinMode(PIN_PELTIER_CONTROL_A, OUTPUT);
  pinMode(PIN_PELTIER_CONTROL_B, OUTPUT);
  digitalWrite(PIN_PELTIER_CONTROL_A, LOW);
  digitalWrite(PIN_PELTIER_CONTROL_B, LOW);
  digitalWrite(PIN_PELTIER_ENABLE, LOW); // disable the h-bridges
  prev_val_a = 0;
  prev_val_b = 0;
}

void Peltiers::set_cold_percentage(double perc){
  //Serial.print(" Cold=");Serial.println(int(perc * 100));
  digitalWrite(PIN_PELTIER_ENABLE, HIGH);
  int val = min(max(perc * 255.0, 0.0), 255.0);
  if (val != prev_val_a) {
    pinMode(PIN_PELTIER_CONTROL_A, OUTPUT);
    pinMode(PIN_PELTIER_CONTROL_B, OUTPUT);
    digitalWrite(PIN_PELTIER_CONTROL_B, LOW);
    prev_val_b = 0;
    if (val == 255) digitalWrite(PIN_PELTIER_CONTROL_A, HIGH);
    else if (val == 0) digitalWrite(PIN_PELTIER_CONTROL_A, LOW);
    else analogWrite(PIN_PELTIER_CONTROL_A, val);
  }
  prev_val_a = val;
}

void Peltiers::set_hot_percentage(double perc){
  //Serial.print(" Hot=");Serial.println(int(perc * 100));
  digitalWrite(PIN_PELTIER_ENABLE, HIGH);
  int val = min(max(perc * 255.0, 0.0), 255.0);
  if (val != prev_val_b) {
    pinMode(PIN_PELTIER_CONTROL_A, OUTPUT);
    pinMode(PIN_PELTIER_CONTROL_B, OUTPUT);
    digitalWrite(PIN_PELTIER_CONTROL_A, LOW);
    prev_val_a = 0;
    if (val == 255) digitalWrite(PIN_PELTIER_CONTROL_B, HIGH);
    else if (val == 0) digitalWrite(PIN_PELTIER_CONTROL_B, LOW);
    else analogWrite(PIN_PELTIER_CONTROL_B, val);
  }
  prev_val_b = val;
}

void Peltiers::setup() {
  pinMode(PIN_PELTIER_CONTROL_A, OUTPUT);
  pinMode(PIN_PELTIER_CONTROL_B, OUTPUT);
  pinMode(PIN_PELTIER_ENABLE, OUTPUT);
  disable();
}
