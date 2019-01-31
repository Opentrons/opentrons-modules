/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

#include <PID_v1.h>
#include "thermistorsadc.h"
#include "lid.h"
#include "peltiers.h"

#define THERMISTOR_VOLTAGE 1.5

ThermistorsADC temp_probes;
Lid lid;
Peltiers peltiers;

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

#include <Adafruit_NeoPixel_ZeroDMA.h>

#define NEO_PWR     4
#define NEO_PIN     A5
#define NUM_PIXELS  22

Adafruit_NeoPixel_ZeroDMA strip(NUM_PIXELS, NEO_PIN, NEO_RGBW);

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

#define PIN_FAN_SINK_CTRL           A4   // uses PWM frequency generator
#define PIN_FAN_SINK_ENABLE         2    // Heat sink fan

#define PIN_HEAT_PAD_CONTROL        A3
#define PIN_FAN_COVER               A2

double CURRENT_FAN_POWER = 0;

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

#define TEMPERATURE_ROOM 23
#define TEMPERATURE_COVER_HOT 105

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

#define DEFAULT_PLATE_PID_TIME 20

#define PID_KP_PLATE_UP 0.05
#define PID_KI_PLATE_UP 0.01
#define PID_KD_PLATE_UP 0.0

#define PID_KP_PLATE_DOWN PID_KP_PLATE_UP
#define PID_KI_PLATE_DOWN PID_KI_PLATE_UP
#define PID_KD_PLATE_DOWN PID_KD_PLATE_UP

#define PID_STABILIZING_THRESH 5
#define PID_FAR_AWAY_THRESH 10

double current_plate_kp = PID_KP_PLATE_UP;
double current_plate_ki = PID_KI_PLATE_UP;
double current_plate_kd = PID_KD_PLATE_UP;

bool MASTER_SET_A_TARGET = false;
bool auto_fan = true;
bool just_changed_temp = false;

double TEMPERATURE_SWING_PLATE = 0.5;
double TARGET_TEMPERATURE_PLATE = TEMPERATURE_ROOM;
double CURRENT_TEMPERATURE_PLATE = TEMPERATURE_ROOM;
//
// PID PID_Plate(
//   &CURRENT_TEMPERATURE_PLATE, &TEMPERATURE_SWING_PLATE, &TARGET_TEMPERATURE_PLATE,
//   current_plate_kp, current_plate_ki, current_plate_kd, P_ON_M, DIRECT);

double TESTING_OFFSET_TEMP = TARGET_TEMPERATURE_PLATE;
double CURRENT_LEFT_PEL_TEMP = TEMPERATURE_ROOM;
double CURRENT_CENTER_PEL_TEMP = TEMPERATURE_ROOM;
double CURRENT_RIGHT_PEL_TEMP = TEMPERATURE_ROOM;
double TEMPERATURE_SWING_LEFT_PEL = 0.5;
double TEMPERATURE_SWING_CENTER_PEL = 0.5;
double TEMPERATURE_SWING_RIGHT_PEL = 0.5;

PID PID_left_pel(
  &CURRENT_LEFT_PEL_TEMP,
  &TEMPERATURE_SWING_LEFT_PEL,
  &TARGET_TEMPERATURE_PLATE,
  current_plate_kp, current_plate_ki, current_plate_kd,
  P_ON_M,
  DIRECT
);

PID PID_center_pel(
  &CURRENT_CENTER_PEL_TEMP,
  &TEMPERATURE_SWING_CENTER_PEL,
  &TARGET_TEMPERATURE_PLATE,
  current_plate_kp, current_plate_ki, current_plate_kd,
  P_ON_M,
  DIRECT
);

PID PID_right_pel(
  &CURRENT_RIGHT_PEL_TEMP,
  &TEMPERATURE_SWING_RIGHT_PEL,
  &TARGET_TEMPERATURE_PLATE,
  current_plate_kp, current_plate_ki, current_plate_kd,
  P_ON_M,
  DIRECT
);

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

#define PID_KP_COVER 0.2
#define PID_KI_COVER 0.01
#define PID_KD_COVER 0.0

double TEMPERATURE_SWING_COVER = 0.5;
double TARGET_TEMPERATURE_COVER = TEMPERATURE_ROOM;
double CURRENT_TEMPERATURE_COVER = TEMPERATURE_ROOM;

bool COVER_SHOULD_BE_HOT = false;

PID PID_Cover(
  &CURRENT_TEMPERATURE_COVER, &TEMPERATURE_SWING_COVER, &TARGET_TEMPERATURE_COVER,
  PID_KP_COVER, PID_KI_COVER, PID_KD_COVER, P_ON_M, DIRECT);

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void set_heat_pad_power(float val) {
  int byte_val = max(min(val * 255.0, 255), 0);
  pinMode(PIN_HEAT_PAD_CONTROL, OUTPUT);
  if (byte_val == 255) digitalWrite(PIN_HEAT_PAD_CONTROL, HIGH);
  else if (byte_val == 0) digitalWrite(PIN_HEAT_PAD_CONTROL, LOW);
  else analogWrite(PIN_HEAT_PAD_CONTROL, byte_val);
}

void heat_pad_off() {
  COVER_SHOULD_BE_HOT = false;
  TEMPERATURE_SWING_COVER = 0.0;
  set_heat_pad_power(0);
}

void heat_pad_on() {
  COVER_SHOULD_BE_HOT = true;
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void fan_cover_on() {
  digitalWrite(PIN_FAN_COVER, LOW);
}

void fan_cover_off() {
  digitalWrite(PIN_FAN_COVER, HIGH);
}

void fan_sink_percentage(float value) {
  pinMode(PIN_FAN_SINK_CTRL, OUTPUT);
  pinMode(PIN_FAN_SINK_ENABLE, OUTPUT);
  digitalWrite(PIN_FAN_SINK_ENABLE, LOW);
  int val = value * 255.0;
  if (val < 0) val = 0;
  else if (val > 255) val = 255;
  CURRENT_FAN_POWER = value;
  analogWrite(PIN_FAN_SINK_CTRL, val);
}

void fan_sink_off() {
  pinMode(PIN_FAN_SINK_CTRL, OUTPUT);
  pinMode(PIN_FAN_SINK_ENABLE, OUTPUT);
  digitalWrite(PIN_FAN_SINK_CTRL, LOW);
  digitalWrite(PIN_FAN_SINK_ENABLE, HIGH);
  CURRENT_FAN_POWER = 0.0;
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void disable_scary_stuff() {
    heat_pad_off();
    fan_sink_off();
    peltiers.disable();
    TARGET_TEMPERATURE_PLATE = TEMPERATURE_ROOM;
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

bool is_stabilizing(Peltier_num pel_n = NO_PELTIER) {
  if (pel_n < 0)
    return abs(TARGET_TEMPERATURE_PLATE - CURRENT_TEMPERATURE_PLATE) < PID_STABILIZING_THRESH;
  else
    return abs(TARGET_TEMPERATURE_PLATE - get_average_temp(pel_n)) < PID_STABILIZING_THRESH;
}

bool is_target_hot() {
  return TARGET_TEMPERATURE_PLATE > TEMPERATURE_ROOM;
}

bool is_target_cold() {
  return TARGET_TEMPERATURE_PLATE < TEMPERATURE_ROOM;
}

bool is_moving_up(Peltier_num pel_n = NO_PELTIER) {
  if (pel_n < 0)
    return TARGET_TEMPERATURE_PLATE > CURRENT_TEMPERATURE_PLATE;
  else
    return TARGET_TEMPERATURE_PLATE > get_average_temp(pel_n);
}

bool is_moving_down(Peltier_num pel_n = NO_PELTIER) {
  if(pel_n < 0)
    return TARGET_TEMPERATURE_PLATE < CURRENT_TEMPERATURE_PLATE;
  else
    return TARGET_TEMPERATURE_PLATE < get_average_temp(pel_n);
}

bool is_ramping_up(Peltier_num pel_n = NO_PELTIER) {
  return (!is_stabilizing(pel_n) && is_moving_up(pel_n));
}

bool is_ramping_down(Peltier_num pel_n = NO_PELTIER) {
  return (!is_stabilizing(pel_n) && is_moving_down(pel_n));
}

bool is_temp_far_away() {
  return abs(TARGET_TEMPERATURE_PLATE - CURRENT_TEMPERATURE_PLATE) > PID_FAR_AWAY_THRESH;
}

float get_average_temp(Peltier_num pel_n) {
  switch(pel_n){
    case PELTIER_1:
      return (temp_probes.front_left_temperature()
              + temp_probes.back_left_temperature())/2;
    case PELTIER_2:
      return (temp_probes.front_center_temperature()
              + temp_probes.back_center_temperature())/2;
    case PELTIER_3:
      return (temp_probes.front_right_temperature()
              + temp_probes.back_right_temperature())/2;
    default:
      Serial.println("ERROR<get_average_temp>! Invalid Peltier number");
      break;
  }
}
/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void update_peltiers_from_pid() {
  if (MASTER_SET_A_TARGET) {
    // if (PID_Plate.Compute()) {
    //   if (TEMPERATURE_SWING_PLATE < 0) {
    //     peltiers.set_cold_percentage(abs(TEMPERATURE_SWING_PLATE), PELTIER_1);
    //   }
    //   else {
    //     peltiers.set_hot_percentage(TEMPERATURE_SWING_PLATE, PELTIER_1);
    //   }
    // }
    if (PID_left_pel.Compute()) {
      if (TEMPERATURE_SWING_LEFT_PEL < 0) {
        peltiers.set_cold_percentage(abs(TEMPERATURE_SWING_LEFT_PEL), PELTIER_1);
      }
      else {
        peltiers.set_hot_percentage(TEMPERATURE_SWING_LEFT_PEL, PELTIER_1);
      }
    }
    if (PID_right_pel.Compute()) {
      if (TEMPERATURE_SWING_RIGHT_PEL < 0) {
        peltiers.set_cold_percentage(abs(TEMPERATURE_SWING_RIGHT_PEL), PELTIER_3);
      }
      else {
        peltiers.set_hot_percentage(TEMPERATURE_SWING_RIGHT_PEL, PELTIER_3);
      }
    }
    if (PID_center_pel.Compute()) {
      if (TEMPERATURE_SWING_CENTER_PEL < 0) {
        peltiers.set_cold_percentage(abs(TEMPERATURE_SWING_CENTER_PEL), PELTIER_2);
      }
      else {
        peltiers.set_hot_percentage(TEMPERATURE_SWING_CENTER_PEL, PELTIER_2);
      }
    }
  }
}

void update_cover_from_pid() {
  if (COVER_SHOULD_BE_HOT) {
    if (PID_Cover.Compute()) {
      set_heat_pad_power(TEMPERATURE_SWING_COVER);
    }
  }
  else {
    heat_pad_off();
  }
}

void update_fan_from_state() {
  if (is_target_cold()) {
    fan_sink_percentage(1.0);
  }
  else if (is_ramping_down()) {
    fan_sink_percentage(0.5);
  }
  else if (is_ramping_up()) {
    fan_sink_percentage(0.2);
  }
  else if (is_stabilizing()) {
    fan_sink_percentage(0.0);
  }
  if (COVER_SHOULD_BE_HOT) {
    fan_cover_on();
  }
  else {
    fan_cover_off();
  }
}

void ramp_temp_after_change_temp() {
  if (is_temp_far_away()) {
    // PID_Plate.SetSampleTime(1);
    // PID_Plate.SetTunings(current_plate_kp, 1.0, current_plate_kd, P_ON_M);
    // if (is_ramping_up() && TEMPERATURE_SWING_PLATE < 0.95) {
    //   peltiers.set_hot_percentage(1.0, PELTIER_1);
    //   while (TEMPERATURE_SWING_PLATE < 0.95) PID_Plate.Compute();
    // }
    // else if (is_ramping_down() && TEMPERATURE_SWING_PLATE > -0.95) {
    //   peltiers.set_cold_percentage(1.0, PELTIER_1);
    //   while (TEMPERATURE_SWING_PLATE > -0.95) PID_Plate.Compute();
    // }
    PID_left_pel.SetSampleTime(1);
    PID_left_pel.SetTunings(current_plate_kp, 1.0, current_plate_kd, P_ON_M);
    if (is_ramping_up(PELTIER_1) && TEMPERATURE_SWING_LEFT_PEL < 0.95){
      peltiers.set_hot_percentage(1.0, PELTIER_1);
      while (TEMPERATURE_SWING_LEFT_PEL < 0.95)
        PID_left_pel.Compute();
    }
    else if (is_ramping_down(PELTIER_1) && TEMPERATURE_SWING_LEFT_PEL > 0.95){
      peltiers.set_cold_percentage(1.0, PELTIER_1);
      while (TEMPERATURE_SWING_LEFT_PEL > -0.95)
        PID_left_pel.Compute();
    }

    PID_center_pel.SetSampleTime(1);
    PID_center_pel.SetTunings(current_plate_kp, 1.0, current_plate_kd, P_ON_M);
    if (is_ramping_up(PELTIER_2) && TEMPERATURE_SWING_CENTER_PEL < 0.95){
      peltiers.set_hot_percentage(1.0, PELTIER_2);
      while (TEMPERATURE_SWING_CENTER_PEL < 0.95)
        PID_center_pel.Compute();
    }
    else if (is_ramping_down(PELTIER_2) && TEMPERATURE_SWING_CENTER_PEL > 0.95){
      peltiers.set_cold_percentage(1.0, PELTIER_2);
      while (TEMPERATURE_SWING_CENTER_PEL > -0.95)
        PID_center_pel.Compute();
    }

    PID_right_pel.SetSampleTime(1);
    PID_right_pel.SetTunings(current_plate_kp, 1.0, current_plate_kd, P_ON_M);
    if (is_ramping_up(PELTIER_3) && TEMPERATURE_SWING_RIGHT_PEL < 0.95){
      peltiers.set_hot_percentage(1.0, PELTIER_3);
      while (TEMPERATURE_SWING_RIGHT_PEL < 0.95)
        PID_right_pel.Compute();
    }
    else if (is_ramping_down(PELTIER_3) && TEMPERATURE_SWING_RIGHT_PEL > 0.95){
      peltiers.set_cold_percentage(1.0, PELTIER_3);
      while (TEMPERATURE_SWING_CENTER_PEL > -0.95)
        PID_right_pel.Compute();
    }
  }
  Serial.println("Setting Tunings!");
  Serial.println(current_plate_kp, 6);
  Serial.println(current_plate_ki, 6);
  Serial.println(current_plate_kd, 6);
  Serial.println();
  PID_left_pel.SetSampleTime(DEFAULT_PLATE_PID_TIME);
  PID_left_pel.SetTunings(current_plate_kp, current_plate_ki, current_plate_kd, P_ON_M);
  PID_center_pel.SetSampleTime(DEFAULT_PLATE_PID_TIME);
  PID_center_pel.SetTunings(current_plate_kp, current_plate_ki, current_plate_kd, P_ON_M);
  PID_right_pel.SetSampleTime(DEFAULT_PLATE_PID_TIME);
  PID_right_pel.SetTunings(current_plate_kp, current_plate_ki, current_plate_kd, P_ON_M);

  just_changed_temp = false;
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

unsigned long plotter_timestamp = 0;
const int plotter_interval = 500;
bool running_from_script = false;
bool debug_print_mode = true;
bool running_graph = false;
bool zoom_mode = false;

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void print_info(bool force=false) {
    if (force || (millis() - plotter_timestamp > plotter_interval)) {
        plotter_timestamp = millis();
        if (zoom_mode) {
          // Serial.print(double(millis()) / 1000.0, 2);
          // Serial.print(',');
          Serial.print(CURRENT_TEMPERATURE_PLATE - TARGET_TEMPERATURE_PLATE);
          // Serial.print(',');
          // Serial.print(CURRENT_TEMPERATURE_COVER - TEMPERATURE_COVER_HOT);
          Serial.println();
          return;
        }
        if (debug_print_mode) Serial.println("\n\n*******");
        if (debug_print_mode) Serial.print("\nAverage Plate-Temp (Heat sink temp):\t\t");
        Serial.print(CURRENT_TEMPERATURE_PLATE);
        if (debug_print_mode){
          Serial.print("\n\t\t-------- Thermistors -------\n");
          Serial.print(temp_probes.front_left_temperature());
          Serial.print("\t");
          Serial.print(temp_probes.front_center_temperature());
          Serial.print("/t");
          Serial.print(temp_probes.front_right_temperature());
          Serial.print("\n\n");
          Serial.print(temp_probes.back_left_temperature());
          Serial.print("\t");
          Serial.print(temp_probes.back_center_temperature());
          Serial.print("\t");
          Serial.print(temp_probes.back_right_temperature());
          Serial.print("\n");
          Serial.print("\t\t-------------------------------");
        }
        if (running_from_script || running_graph) Serial.print(" ");
        if (debug_print_mode) Serial.print("\n\tPlate-Target:\t");
        if (!MASTER_SET_A_TARGET && debug_print_mode) Serial.print("off");
        else Serial.print(TARGET_TEMPERATURE_PLATE);
        if (running_from_script || running_graph) Serial.print(" ");
        if (debug_print_mode) Serial.print("\n\tPlate-PID:\t");
        if (!MASTER_SET_A_TARGET && debug_print_mode) Serial.print("off");
        else Serial.print((TEMPERATURE_SWING_PLATE * 50.0) + 50.0);
        if (running_from_script || running_graph) Serial.print(" ");
        if (debug_print_mode) Serial.print("\nCover-Temp:\t\t");
        Serial.print(CURRENT_TEMPERATURE_COVER);
        if (running_from_script || running_graph) Serial.print(" ");
        if (debug_print_mode) Serial.print("\n\tCover-Target:\t");
        if (!COVER_SHOULD_BE_HOT && debug_print_mode) Serial.print("off");
        else Serial.print(TEMPERATURE_COVER_HOT);
        if (running_from_script || running_graph) Serial.print(" ");
        if (debug_print_mode) Serial.print("\n\tCover-PID:\t");
        if (!COVER_SHOULD_BE_HOT && debug_print_mode) Serial.print("off");
        else Serial.print(TEMPERATURE_SWING_COVER * 100.0);
        if (running_from_script || running_graph) Serial.print(" ");
        if (debug_print_mode) Serial.print("\nFan Power:\t\t");
        Serial.print(CURRENT_FAN_POWER * 100);
        Serial.println();
    }
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void empty_serial_buffer() {
    if (running_from_script){
        Serial.println("ok");
    }
    while (Serial.available()){
      Serial.read();
    }
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void read_from_serial() {
    if (Serial.available() > 0){
        if (Serial.peek() == 'x') {
            MASTER_SET_A_TARGET = false;
            COVER_SHOULD_BE_HOT = false;
            disable_scary_stuff();
        }
        else if (Serial.peek() == 'd') {
            running_from_script = false;
            debug_print_mode = true;
            running_graph = false;
            zoom_mode = false;
        }
        else if (Serial.peek() == 's') {
            running_from_script = true;
            debug_print_mode = false;
            running_graph = false;
            zoom_mode = false;
        }
        else if (Serial.peek() == 'g') {
            running_from_script = false;
            debug_print_mode = false;
            running_graph = true;
            zoom_mode = false;
        }
        else if (Serial.peek() == 'z') {
            running_from_script = false;
            debug_print_mode = false;
            running_graph = false;
            zoom_mode = true;
        }
        else if (Serial.peek() == 'c') {
            Serial.read();
            if (Serial.peek() == '1') {
              heat_pad_on();
            }
            else {
              heat_pad_off();
            }
        }
        else if (Serial.peek() == 't') {
            Serial.read();
            MASTER_SET_A_TARGET = true;
            TARGET_TEMPERATURE_PLATE = Serial.parseFloat();
            TESTING_OFFSET_TEMP = TARGET_TEMPERATURE_PLATE;
            if (Serial.peek() == 'o') {
              Serial.read();
              TARGET_TEMPERATURE_PLATE -= Serial.parseFloat();
              Serial.print("Setting Offset to ");
              Serial.println(TARGET_TEMPERATURE_PLATE, 6);
            }
            if (Serial.peek() == 'p') {
              Serial.read();
              current_plate_kp = Serial.parseFloat();
              Serial.print("Setting P to ");
              Serial.println(current_plate_kp, 6);
            }
            else {
              if (is_moving_up()) current_plate_kp = PID_KP_PLATE_UP;
              else current_plate_kp = PID_KP_PLATE_DOWN;
            }
            if (Serial.peek() == 'i') {
              Serial.read();
              current_plate_ki = Serial.parseFloat();
              Serial.print("Setting I to ");
              Serial.println(current_plate_ki, 6);
            }
            else {
              if (is_moving_up()) current_plate_ki = PID_KI_PLATE_UP;
              else current_plate_ki = PID_KI_PLATE_DOWN;
            }
            if (Serial.peek() == 'd') {
              Serial.read();
              current_plate_kd = Serial.parseFloat();
              Serial.print("Setting D to ");
              Serial.println(current_plate_kd, 6);
            }
            else {
              if (is_moving_up()) current_plate_kd = PID_KD_PLATE_UP;
              else current_plate_kd = PID_KD_PLATE_DOWN;
            }
            just_changed_temp = true;
        }
        else if (Serial.peek() == 'f') {
            Serial.read();
            if (!Serial.available() || Serial.peek() == '\n' || Serial.peek() == '\r') {
              auto_fan = true;
            }
            else {
              auto_fan = false;
              float percentage = Serial.parseFloat();
              fan_sink_percentage(percentage / 100.0);
            }
        }
        else if (Serial.peek() == 'p') {
            if (running_from_script) print_info(true);
        }
        empty_serial_buffer();
    }
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

uint32_t startTime;

void rainbow_test() {
  // Rainbow cycle
  uint32_t elapsed = micros() - startTime;
  for(int i=0; i<strip.numPixels(); i++) {
    strip.setPixelColor(i, Wheel((uint8_t)(
      (elapsed * 256 / 1000000) + i * 256 / strip.numPixels())));
  }
  strip.show();
}

uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) {
    return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if(WheelPos < 170) {
    WheelPos -= 85;
    return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void setup() {

  Serial.begin(115200);
  Serial.setTimeout(5);

  lid.setup();
  peltiers.setup();

  temp_probes.setup(THERMISTOR_VOLTAGE);
  while (!temp_probes.update()) {}
  CURRENT_TEMPERATURE_PLATE = temp_probes.average_plate_temperature();
  CURRENT_TEMPERATURE_COVER = temp_probes.cover_temperature();

  pinMode(PIN_FAN_SINK_CTRL, OUTPUT);
  pinMode(PIN_FAN_COVER, OUTPUT);
  fan_sink_off();
  fan_cover_off();

  heat_pad_off();

  PID_left_pel.SetSampleTime(DEFAULT_PLATE_PID_TIME);
  PID_left_pel.SetTunings(current_plate_kp, current_plate_ki, current_plate_kd, P_ON_M);
  PID_left_pel.SetMode(AUTOMATIC);
  PID_left_pel.SetOutputLimits(-1.0, 1.0);
  PID_center_pel.SetSampleTime(DEFAULT_PLATE_PID_TIME);
  PID_center_pel.SetTunings(current_plate_kp, current_plate_ki, current_plate_kd, P_ON_M);
  PID_center_pel.SetMode(AUTOMATIC);
  PID_center_pel.SetOutputLimits(-1.0, 1.0);
  PID_right_pel.SetSampleTime(DEFAULT_PLATE_PID_TIME);
  PID_right_pel.SetTunings(current_plate_kp, current_plate_ki, current_plate_kd, P_ON_M);
  PID_right_pel.SetMode(AUTOMATIC);
  PID_right_pel.SetOutputLimits(-1.0, 1.0);

  PID_Cover.SetMode(AUTOMATIC);
  PID_Cover.SetSampleTime(250);
  PID_Cover.SetOutputLimits(0.0, 1.0);

  while (!PID_left_pel.Compute()) {}
  while (!PID_center_pel.Compute()) {}
  while (!PID_right_pel.Compute()) {}
  while (!PID_Cover.Compute()) {}

  TARGET_TEMPERATURE_PLATE = TEMPERATURE_ROOM;
  TARGET_TEMPERATURE_COVER = TEMPERATURE_COVER_HOT;
  MASTER_SET_A_TARGET = false;
  COVER_SHOULD_BE_HOT = false;

  strip.begin();
  strip.setBrightness(32);
  strip.show();
  startTime = micros();  // for the rainbow test
  rainbow_test();
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void loop() {

  read_from_serial();

  if (temp_probes.update()) {
    CURRENT_LEFT_PEL_TEMP = temp_probes.left_pair_temperature();
    CURRENT_RIGHT_PEL_TEMP = temp_probes.right_pair_temperature();
    CURRENT_CENTER_PEL_TEMP = temp_probes.center_pair_temperature();
    CURRENT_TEMPERATURE_PLATE = temp_probes.average_plate_temperature();
    CURRENT_TEMPERATURE_COVER = temp_probes.cover_temperature();
  }

  if (auto_fan) update_fan_from_state();
  if (just_changed_temp) ramp_temp_after_change_temp();
  update_peltiers_from_pid();
  update_cover_from_pid();

  if (!running_from_script) print_info();
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////
