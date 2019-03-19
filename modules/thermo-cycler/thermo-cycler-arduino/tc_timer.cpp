#include "Arduino.h"
#include "tc_timer.h"

TC_Timer::TC_Timer() {}

void TC_Timer::reset()
{
  total_hold_time = 0;
  _status = Timer_status::idle;
  _total_hold_time_millis = 0;
  _hold_start_timestamp = 0;
}

bool TC_Timer::start()
{
  if(_status == Timer_status::running)
  {
    return false;
  }
    _total_hold_time_millis = total_hold_time * 1000;
    _hold_start_timestamp = millis();
    _status = Timer_status::running;
    return true;
}

unsigned int TC_Timer::time_left()
{
  update();
  if(_status == Timer_status::running)
  {
    return (_total_hold_time_millis - _elapsed_time) / 1000;
  }
  else if(_status == Timer_status::complete)
  {
    return 0;
  }
  else
  {
    return total_hold_time;
  }
}

void TC_Timer::update()
{
  if(_status == Timer_status::running)
  {
    _elapsed_time = millis() - _hold_start_timestamp;
    if (_elapsed_time >= _total_hold_time_millis)
    {
      _status = Timer_status::complete;
    }
  }
}

Timer_status TC_Timer::status()
{
  return _status;
}
