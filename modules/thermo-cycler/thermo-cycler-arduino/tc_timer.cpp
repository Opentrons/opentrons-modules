#include "Arduino.h"
#include "tc_timer.h"
// #include "limits.h"

TC_Timer::TC_Timer() {}

void TC_Timer::reset() {
  total_hold_time = 0;
  _status = IDLE;
  _total_hold_time_millis = 0;
  _hold_start_timestamp = 0;
}

bool TC_Timer::start() {
  if(_status == RUNNING) {
    return false;
  }
    _total_hold_time_millis = total_hold_time * 1000;
    _hold_start_timestamp = millis();
    _status = RUNNING;
    return true;
}

unsigned long TC_Timer::time_left() {
  update();
  if(_status == RUNNING){
    return (_total_hold_time_millis - _elapsed_time) / 1000;
  }
  else if(_status == COMPLETE){
    return 0;
  }
  else{
    return total_hold_time;
  }
}

void TC_Timer::update() {
  if(_status == RUNNING) {
    _elapsed_time = millis() - _hold_start_timestamp;
    if (_elapsed_time >= _total_hold_time_millis) {
      _status = COMPLETE;
    }
  }
}

Timer_status TC_Timer::get_status() {
  return _status;
}
