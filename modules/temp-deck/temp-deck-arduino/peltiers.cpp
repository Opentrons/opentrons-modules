#include "Peltiers.h"

Peltiers::Peltiers(){
}

void Peltiers::set_peltiers_percentage(float a_state, float b_state){
  peltier_on_time = int(float(max(a_state, b_state)) * float(PELTIER_CYCLE_MS));
  peltier_off_time = int(PELTIER_CYCLE_MS) - peltier_on_time;
//  Serial.print("Peltier duty cycle -> ");Serial.println(peltier_duty_cycle_ms);
  if (a_state > 0.0){
    peltier_high_pin = PELTIER_A_CONTROL;
    peltier_low_pin = PELTIER_B_CONTROL;
    digitalWrite(PELTIER_AB_ENABLE, HIGH);  // enable the h-bridges
    enabled = true;
  }
  else if (b_state > 0.0){
    peltier_high_pin = PELTIER_B_CONTROL;
    peltier_low_pin = PELTIER_A_CONTROL;
    digitalWrite(PELTIER_AB_ENABLE, HIGH);  // enable the h-bridges
    enabled = true;
  }
  else {
    write_h_bridges(LOW);
    digitalWrite(PELTIER_AB_ENABLE, LOW); // disable the h-bridges
    enabled = false;
  }
}

void Peltiers::update_peltier_cycle() {
  if (enabled == false || (peltier_on_time == 0 && peltiers_currently_on)){
    disable_peltiers();
    return;
  }
  now = millis();
  if (peltiers_currently_on == false) {
    if (now > peltier_cycle_timestamp + peltier_off_time) {
      peltier_cycle_timestamp = now;
      write_h_bridges(HIGH);
    }
  }
  else {
    if (now > peltier_cycle_timestamp + peltier_on_time) {
      peltier_cycle_timestamp = now;
      if (peltier_off_time > 0){  // incase we've set it to 100%
        write_h_bridges(LOW);
      }
    }
  }
}

void Peltiers::write_h_bridges(int state) {
  digitalWrite(peltier_high_pin, state);
  digitalWrite(peltier_low_pin, LOW);
  peltiers_currently_on = boolean(state);
//  Serial.print("Writing pin ");Serial.print(peltier_high_pin);Serial.print(" -> ");Serial.println(state);
}

void Peltiers::disable_peltiers(){
//  Serial.println("Peltiers are both OFF");
  set_peltiers_percentage(0.0, 0.0);
}

void Peltiers::set_cold_percentage(float perc){
  set_peltiers_percentage(perc, 0);
}

void Peltiers::set_hot_percentage(float perc){
  set_peltiers_percentage(0, perc);
}

void Peltiers::setup_peltiers() {
  pinMode(PELTIER_A_CONTROL, OUTPUT);
  pinMode(PELTIER_B_CONTROL, OUTPUT);
  pinMode(PELTIER_AB_ENABLE, OUTPUT);
  disable_peltiers();
}
