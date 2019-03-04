#ifndef TC_TIMER_H_
#define TC_TIMER_H_

typedef enum{
  IDLE,
  RUNNING,
  COMPLETE
}Timer_status;

class TC_Timer{
  public:
    unsigned int total_hold_time;
    TC_Timer();
    void reset();
    bool start();
    unsigned long time_left();
    Timer_status get_status();
    void update();
  private:
    unsigned long _total_hold_time_millis = 0;
    unsigned long _hold_start_timestamp = 0;
    unsigned long _elapsed_time = 0;
    Timer_status _status;
};
#endif
