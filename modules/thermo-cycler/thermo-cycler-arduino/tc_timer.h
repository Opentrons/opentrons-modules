#ifndef TC_TIMER_H_
#define TC_TIMER_H_

enum class Timer_status
{
  idle,
  running,
  complete
};

class TC_Timer
{
  public:
    unsigned int total_hold_time;
    TC_Timer();
    void reset();
    bool start();
    unsigned int time_left();
    Timer_status status();
    void update();
  private:
    unsigned long _total_hold_time_millis = 0;
    unsigned long _hold_start_timestamp = 0;
    unsigned long _elapsed_time = 0;
    Timer_status _status;
};
#endif
