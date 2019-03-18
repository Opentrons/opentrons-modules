#include "peltiers.h"

Peltiers::Peltiers(){}

void Peltiers::disable()
{
  for(int n = 0; n < PEL_INT(Peltier::max_num); n++)
  {
    pinMode(pel[n].pin_a, OUTPUT);
    pinMode(pel[n].pin_b, OUTPUT);
    digitalWrite(pel[n].pin_a, LOW);
    digitalWrite(pel[n].pin_b, LOW);
    pel[n].prev_val_a = 0;
    pel[n].prev_val_b = 0;
  }
  digitalWrite(PIN_PELTIER_ENABLE, LOW); // disable the h-bridges
}

void Peltiers::set_cold_percentage(double perc, Peltier pel_enum)
{
  int n = PEL_INT(pel_enum);
  digitalWrite(PIN_PELTIER_ENABLE, HIGH);
  int val = min(max(perc * 255.0, 0.0), 255.0);
  if (val != pel[n].prev_val_a)
  {
    pinMode(pel[n].pin_a, OUTPUT);
    pinMode(pel[n].pin_b, OUTPUT);
    digitalWrite(pel[n].pin_b, LOW);
    pel[n].prev_val_b = 0;
    if (val == 255) digitalWrite(pel[n].pin_a, HIGH);
    else if (val == 0) digitalWrite(pel[n].pin_a, LOW);
    else analogWrite(pel[n].pin_a, val);
  }
  pel[n].prev_val_a = val;
}

void Peltiers::set_hot_percentage(double perc, Peltier pel_enum)
{
  int n = PEL_INT(pel_enum);
  digitalWrite(PIN_PELTIER_ENABLE, HIGH);
  int val = min(max(perc * 255.0, 0.0), 255.0);
  if (val != pel[n].prev_val_b)
  {
    pinMode(pel[n].pin_a, OUTPUT);
    pinMode(pel[n].pin_b, OUTPUT);
    digitalWrite(pel[n].pin_a, LOW);
    pel[n].prev_val_a = 0;
    if (val == 255) digitalWrite(pel[n].pin_b, HIGH);
    else if (val == 0) digitalWrite(pel[n].pin_b, LOW);
    else analogWrite(pel[n].pin_b, val);
  }
  pel[n].prev_val_b = val;
}

void Peltiers::setup()
{
  pel[PEL_INT(Peltier::pel_1)] = {PIN_PELTIER_CONTROL_1A, PIN_PELTIER_CONTROL_1B, 0, 0};
  pel[PEL_INT(Peltier::pel_2)] = {PIN_PELTIER_CONTROL_2A, PIN_PELTIER_CONTROL_2B, 0, 0};
  pel[PEL_INT(Peltier::pel_3)] = {PIN_PELTIER_CONTROL_3A, PIN_PELTIER_CONTROL_3B, 0, 0};

  for(int n = 0; n < PEL_INT(Peltier::max_num); n++)
  {
    pinMode(pel[n].pin_a, OUTPUT);
    pinMode(pel[n].pin_b, OUTPUT);
  }
  pinMode(PIN_PELTIER_ENABLE, OUTPUT);
  disable();
}
