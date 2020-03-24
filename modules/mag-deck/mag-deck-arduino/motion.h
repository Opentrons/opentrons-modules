#ifndef Motion_h
#define Motion_h

#define MAX_TRAVEL_DISTANCE_MM 40

#define CURRENT_HIGH 0.4
#define CURRENT_LOW 0.04
#define SET_CURRENT_DELAY_MS 20
#define ENABLE_DELAY_MS 20

#define HOMING_RETRACT 2

#define ACCELERATION_STARTING_DELAY_MICROSECONDS 2000
#define DEFAULT_ACCELERATION_DELAY_FEEDBACK 0.992  // smaller number means faster acceleration
#define PULSE_HIGH_MICROSECONDS 2

#define STPES_PER_ACCELERATION_CYCLE = ACCELERATION_STARTING_DELAY_MICROSECONDS / DEFAULT_ACCELERATION_DELAY_FEEDBACK

#define ACCELERATE_OFF 0
#define ACCELERATE_DOWN 1
#define ACCELERATE_UP 2

struct MotionParams {
  MotionParams(unsigned int model_version);
  MotionParams();

  unsigned int steps_per_mm;
  unsigned long step_delay_microseconds;

  float speed_high;
  float speed_low;
  float speed_probe;

  float mm_per_sec;
  float acceleration_delay_feedback;
  float acceleration_delay_microseconds;

  float current_position_mm;
  float saved_position_offset;
  float found_height;

  uint8_t accelerate_direction;
  float acceleration_factor;
  unsigned long number_of_acceleration_steps;

  void set_speed(float new_speed) {
    //  Serial.print("\tSpeed: ");Serial.println(mm_per_sec);
    mm_per_sec = new_speed;
    step_delay_microseconds = 1000000 / (steps_per_mm * mm_per_sec);
    step_delay_microseconds -= PULSE_HIGH_MICROSECONDS;
  }

  void acceleration_reset(float factor=1.0) {
    acceleration_factor = factor;
    acceleration_delay_microseconds = ACCELERATION_STARTING_DELAY_MICROSECONDS - step_delay_microseconds;
    acceleration_delay_feedback = DEFAULT_ACCELERATION_DELAY_FEEDBACK / factor;
    accelerate_direction = ACCELERATE_UP;
    number_of_acceleration_steps = 0;
  }

  int get_next_acceleration_delay() {
    if (accelerate_direction == ACCELERATE_UP) {
        acceleration_delay_microseconds *= acceleration_delay_feedback;
        number_of_acceleration_steps += 1;
        if(acceleration_delay_microseconds < 0) {
        acceleration_delay_microseconds = 0;
        accelerate_direction = ACCELERATE_OFF;
        }
    }
    else if (accelerate_direction == ACCELERATE_DOWN){
        acceleration_delay_microseconds *= 1.0 + (1.0 - acceleration_delay_feedback);
        if(acceleration_delay_microseconds > ACCELERATION_STARTING_DELAY_MICROSECONDS - step_delay_microseconds) {
        acceleration_delay_microseconds = ACCELERATION_STARTING_DELAY_MICROSECONDS - step_delay_microseconds;
        accelerate_direction = ACCELERATE_OFF;
        }
    }
    return acceleration_delay_microseconds;
  }

  void enable_deceleration_if_needed(int current_step, int total_steps) {
    if (total_steps - current_step <= number_of_acceleration_steps) {
        accelerate_direction = ACCELERATE_DOWN;
    }
  }
};

MotionParams::MotionParams(unsigned int model_version) :
  steps_per_mm(model_version < 20 ? 50 : 100),
  step_delay_microseconds(1000000 / (steps_per_mm * 10)),
  speed_high(model_version < 20 ? 50 : 25),
  speed_low(model_version < 20 ? 15 : 7.5),
  speed_probe(model_version < 20 ? 10 : 5),
  mm_per_sec(speed_low),
  acceleration_delay_feedback(DEFAULT_ACCELERATION_DELAY_FEEDBACK),
  acceleration_delay_microseconds(ACCELERATION_STARTING_DELAY_MICROSECONDS),
  current_position_mm(0),
  saved_position_offset(0),
  found_height(MAX_TRAVEL_DISTANCE_MM - 15),
  accelerate_direction(ACCELERATE_OFF),
  acceleration_factor(1),
  number_of_acceleration_steps(0)
{}

MotionParams::MotionParams(): MotionParams(0) {}

#endif