#include "Peltiers.h"

Peltiers::Peltiers(){
}

void Peltiers::set_peltiers_percentage(float a_state, float b_state){
  peltier_on_time = int(float(max(a_state, b_state)) * float(PELTIER_CYCLE_MS));
  peltier_off_time = int(PELTIER_CYCLE_MS) - peltier_on_time;
//  Serial.print("Peltier duty cycle -> ");Serial.println(peltier_duty_cycle_ms);
  if (a_state > 0.0){
    peltier_high_pin = PELTIER_A_CONTROL;
    digitalWrite(PELTIER_B_CONTROL, LOW);
    digitalWrite(PELTIER_AB_ENABLE, HIGH);  // enable the h-bridges
    enabled = true;
  }
  else if (b_state > 0.0){
    peltier_high_pin = PELTIER_B_CONTROL;
    digitalWrite(PELTIER_A_CONTROL, LOW);
    digitalWrite(PELTIER_AB_ENABLE, HIGH);  // enable the h-bridges
    enabled = true;
  }
}

void Peltiers::update_peltier_cycle() {
  if (enabled == false){
    peltiers_currently_on = false;
  }
  else if (peltier_off_time == 0 && peltier_on_time > 0) {
    peltiers_currently_on = true;
  }
  else if (peltier_on_time == 0) {
    peltiers_currently_on = false;
  }
  else {
    now = millis();
    if (peltier_cycle_timestamp > now) peltier_cycle_timestamp = now;  // rollover
    if (peltiers_currently_on) {
      if (now > peltier_cycle_timestamp + peltier_on_time) {
        peltier_cycle_timestamp = now;
        peltiers_currently_on = false;
      }
    }
    else {
      if (now > peltier_cycle_timestamp + peltier_off_time) {
        peltier_cycle_timestamp = now;
        peltiers_currently_on = true;
      }
    }
  }
  digitalWrite(peltier_high_pin, peltiers_currently_on);
}

void Peltiers::disable_peltiers(){
//  Serial.println("Peltiers are both OFF");
  set_peltiers_percentage(0.0, 0.0);
  digitalWrite(PELTIER_A_CONTROL, LOW);
  digitalWrite(PELTIER_B_CONTROL, LOW);
  digitalWrite(PELTIER_AB_ENABLE, LOW); // disable the h-bridges
  enabled = false;
}

void Peltiers::set_cold_percentage(float perc){
  //Serial.print(" Cold=");Serial.println(int(perc * 100));
  set_peltiers_percentage(perc, 0);
}

void Peltiers::set_hot_percentage(float perc){
  //Serial.print(" Hot=");Serial.println(int(perc * 100));
  set_peltiers_percentage(0, perc);
}

void Peltiers::setup_peltiers() {
  pinMode(PELTIER_A_CONTROL, OUTPUT);
  pinMode(PELTIER_B_CONTROL, OUTPUT);
  pinMode(PELTIER_AB_ENABLE, OUTPUT);
  disable_peltiers();
}
