#include "thermo-cycler.h"

TC_Timer tc_timer;
GcodeHandler gcode;
ThermistorsADC temp_probes;
Lid lid;
Peltiers peltiers;

Adafruit_NeoPixel_ZeroDMA strip(NUM_PIXELS, NEO_PIN, NEO_RGBW);

// Left Peltier -> Peltier::pel_3
PID PID_left_pel(
  &current_left_pel_temp,
  &temperature_swing_left_pel,
  &target_temperature_plate,
  current_plate_kp, current_plate_ki, current_plate_kd,
  P_ON_M,
  DIRECT
);

// Center Peltier -> Peltier::pel_2
PID PID_center_pel(
  &current_center_pel_temp,
  &temperature_swing_center_pel,
  &target_temperature_plate,
  current_plate_kp, current_plate_ki, current_plate_kd,
  P_ON_M,
  DIRECT
);

// Right Peltier -> Peltier::pel_1
PID PID_right_pel(
  &current_right_pel_temp,
  &temperature_swing_right_pel,
  &target_temperature_plate,
  current_plate_kp, current_plate_ki, current_plate_kd,
  P_ON_M,
  DIRECT
);

PID PID_Cover(
  &current_temperature_cover, &temperature_swing_cover, &target_temperature_cover,
  PID_KP_COVER, PID_KI_COVER, PID_KD_COVER, P_ON_M, DIRECT);

/***************************************/

void set_heat_pad_power(float val)
{
  int byte_val = max(min(val * 255.0, 255), 0);
  pinMode(PIN_HEAT_PAD_CONTROL, OUTPUT);
  if (byte_val == 255) digitalWrite(PIN_HEAT_PAD_CONTROL, HIGH);
  else if (byte_val == 0) digitalWrite(PIN_HEAT_PAD_CONTROL, LOW);
  else analogWrite(PIN_HEAT_PAD_CONTROL, byte_val);
}

void heat_pad_off()
{
  cover_should_be_hot = false;
  temperature_swing_cover = 0.0;
  set_heat_pad_power(0);
}

void heat_pad_on()
{
  cover_should_be_hot = true;
}

void fan_cover_on()
{
  digitalWrite(PIN_FAN_COVER, LOW);
}

void fan_cover_off()
{
  digitalWrite(PIN_FAN_COVER, HIGH);
}

void fan_sink_percentage(float value)
{
  pinMode(PIN_FAN_SINK_CTRL, OUTPUT);
  pinMode(PIN_FAN_SINK_ENABLE, OUTPUT);
  digitalWrite(PIN_FAN_SINK_ENABLE, LOW);
  int val = value * 255.0;
  if (val < 0) val = 0;
  else if (val > 255) val = 255;
  current_fan_power = value;
  analogWrite(PIN_FAN_SINK_CTRL, val);
}

void fan_sink_off()
{
  pinMode(PIN_FAN_SINK_CTRL, OUTPUT);
  pinMode(PIN_FAN_SINK_ENABLE, OUTPUT);
  digitalWrite(PIN_FAN_SINK_CTRL, LOW);
  digitalWrite(PIN_FAN_SINK_ENABLE, HIGH);
  current_fan_power = 0.0;
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void disable_scary_stuff()
{
    heat_pad_off();
    fan_sink_off();
    peltiers.disable();
    target_temperature_plate = TEMPERATURE_ROOM;
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

bool is_stabilizing(Peltier pel_n = Peltier::no_peltier)
{
  if (pel_n == Peltier::no_peltier)
  {
    return abs(target_temperature_plate - current_temperature_plate) < PID_STABILIZING_THRESH;
  }
  else
  {
    return abs(target_temperature_plate - get_average_temp(pel_n)) < PID_STABILIZING_THRESH;
  }
}

bool is_at_target()
{
  return abs(
    target_temperature_plate - temp_probes.average_plate_temperature()) \
    < TARGET_TEMP_TOLERANCE;
}

bool is_target_hot()
{
  return target_temperature_plate > TEMPERATURE_ROOM;
}

bool is_target_cold()
{
  return target_temperature_plate < TEMPERATURE_ROOM;
}

bool is_moving_up(Peltier pel_n = Peltier::no_peltier)
{
  if (pel_n == Peltier::no_peltier)
  {
    return target_temperature_plate > current_temperature_plate;
  }
  else
  {
    return target_temperature_plate > get_average_temp(pel_n);
  }
}

bool is_moving_down(Peltier pel_n = Peltier::no_peltier)
{
  if(pel_n == Peltier::no_peltier)
  {
    return target_temperature_plate < current_temperature_plate;
  }
  else
  {
    return target_temperature_plate < get_average_temp(pel_n);
  }
}

bool is_ramping_up(Peltier pel_n = Peltier::no_peltier)
{
  return (!is_stabilizing(pel_n) && is_moving_up(pel_n));
}

bool is_ramping_down(Peltier pel_n = Peltier::no_peltier)
{
  return (!is_stabilizing(pel_n) && is_moving_down(pel_n));
}

bool is_temp_far_away()
{
  return abs(target_temperature_plate - current_temperature_plate) > PID_FAR_AWAY_THRESH;
}

float get_average_temp(Peltier pel_n)
{
  switch(pel_n)
  {
    case Peltier::pel_1:
      return temp_probes.right_pair_temperature();
    case Peltier::pel_2:
      return temp_probes.center_pair_temperature();
    case Peltier::pel_3:
      return temp_probes.left_pair_temperature();
    default:
      Serial.println("ERROR<get_average_temp>! Invalid Peltier number");
      break;
  }
}

void update_peltiers_from_pid()
{
  if (master_set_a_target)
  {
    if (PID_left_pel.Compute())
    {
      if (temperature_swing_left_pel < 0)
      {
        peltiers.set_cold_percentage(abs(temperature_swing_left_pel), Peltier::pel_3);
      }
      else
      {
        peltiers.set_hot_percentage(temperature_swing_left_pel, Peltier::pel_3);
      }
    }
    if (PID_right_pel.Compute())
    {
      if (temperature_swing_right_pel < 0)
      {
        peltiers.set_cold_percentage(abs(temperature_swing_right_pel), Peltier::pel_1);
      }
      else
      {
        peltiers.set_hot_percentage(temperature_swing_right_pel, Peltier::pel_1);
      }
    }
    if (PID_center_pel.Compute())
    {
      if (temperature_swing_center_pel < 0)
      {
        peltiers.set_cold_percentage(abs(temperature_swing_center_pel), Peltier::pel_2);
      }
      else
      {
        peltiers.set_hot_percentage(temperature_swing_center_pel, Peltier::pel_2);
      }
    }
  }
}

void update_cover_from_pid()
{
  if (cover_should_be_hot)
  {
    if (PID_Cover.Compute())
    {
      set_heat_pad_power(temperature_swing_cover);
    }
  }
  else
  {
    heat_pad_off();
  }
}

void update_fan_from_state()
{
  if (is_target_cold())
  {
    fan_sink_percentage(1.0);
  }
  else if (is_ramping_down())
  {
    fan_sink_percentage(0.5);
  }
  else if (is_ramping_up())
  {
    fan_sink_percentage(0.2);
  }
  else if (is_stabilizing())
  {
    fan_sink_percentage(0.0);
  }
  if (cover_should_be_hot)
  {
    fan_cover_on();
  }
  else
  {
    fan_cover_off();
  }
}

void ramp_temp_after_change_temp()
{
  if (is_temp_far_away())
  {
    PID_left_pel.SetSampleTime(1);
    PID_left_pel.SetTunings(current_plate_kp, 1.0, current_plate_kd, P_ON_M);
    if (is_ramping_up(Peltier::pel_3) && temperature_swing_left_pel < 0.95)
    {
      peltiers.set_hot_percentage(1.0, Peltier::pel_3);
      while (temperature_swing_left_pel < 0.95)
        PID_left_pel.Compute();
    }
    else if (is_ramping_down(Peltier::pel_3) && temperature_swing_left_pel > 0.95)
    {
      peltiers.set_cold_percentage(1.0, Peltier::pel_3);
      while (temperature_swing_left_pel > -0.95)
        PID_left_pel.Compute();
    }

    PID_center_pel.SetSampleTime(1);
    PID_center_pel.SetTunings(current_plate_kp, 1.0, current_plate_kd, P_ON_M);
    if (is_ramping_up(Peltier::pel_2) && temperature_swing_center_pel < 0.95)
    {
      peltiers.set_hot_percentage(1.0, Peltier::pel_2);
      while (temperature_swing_center_pel < 0.95)
        PID_center_pel.Compute();
    }
    else if (is_ramping_down(Peltier::pel_2) && temperature_swing_center_pel > 0.95)
    {
      peltiers.set_cold_percentage(1.0, Peltier::pel_2);
      while (temperature_swing_center_pel > -0.95)
        PID_center_pel.Compute();
    }

    PID_right_pel.SetSampleTime(1);
    PID_right_pel.SetTunings(current_plate_kp, 1.0, current_plate_kd, P_ON_M);
    if (is_ramping_up(Peltier::pel_1) && temperature_swing_right_pel < 0.95)
    {
      peltiers.set_hot_percentage(1.0, Peltier::pel_1);
      while (temperature_swing_right_pel < 0.95)
        PID_right_pel.Compute();
    }
    else if (is_ramping_down(Peltier::pel_1) && temperature_swing_right_pel > 0.95)
    {
      peltiers.set_cold_percentage(1.0, Peltier::pel_1);
      while (temperature_swing_right_pel > -0.95)
        PID_right_pel.Compute();
    }
  }
  /******** Temperature stabilizing towards target *********/
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

#if USE_GCODES == true
  void read_gcode()
  {
    if(gcode.received_newline())
    {
      while(!gcode.buffer_empty())
      {
        switch(gcode.get_command())
        {
          case Gcode::no_code:
            break;
          case Gcode::get_lid_status:
            gcode.response("Lid", lid.LID_STATUS_STRINGS[static_cast<int>(lid.status())]);
            break;
          case Gcode::open_lid:
            lid.open_cover();
            break;
          case Gcode::close_lid:
            lid.close_cover();
            break;
          case Gcode::set_lid_temp:
            break;
          case Gcode::deactivate_lid_heating:
            heat_pad_off();
            break;
          case Gcode::set_plate_temp:
            if (!gcode.pop_arg('S'))
            {
              gcode.response("ERROR", "Arg error");
              break;
            }
            target_temperature_plate = gcode.popped_arg();
            master_set_a_target = true;
            just_changed_temp = true;
            tc_timer.reset();
            // Check if hold time is specified
            if (gcode.pop_arg('H'))
            {
              tc_timer.total_hold_time = gcode.popped_arg();
            }
            break;
          case Gcode::get_plate_temp:
            if(master_set_a_target)
            {
              gcode.targetting_temperature_response(target_temperature_plate,
              current_temperature_plate, tc_timer.time_left());
            }
            else
            {
              gcode.idle_temperature_response(current_temperature_plate);
            }
            break;
          case Gcode::set_ramp_rate:
            if(master_set_a_target)
            {
              gcode.response("ERROR", "BUSY");
            }
            else
            {
              // This should calculate and change PID sample time and Ki
            }
            break;
          case Gcode::edit_pid_params:
            if(master_set_a_target)
            {
              gcode.response("ERROR", "BUSY");
            }
            else
            {
              // Currently, all three peltiers use the same PID values
              if(gcode.pop_arg('P'))
              {
                current_plate_kp = gcode.popped_arg();
              }
              if(gcode.pop_arg('I'))
              {
                current_plate_ki = gcode.popped_arg();
              }
              if (gcode.pop_arg('D'))
              {
                current_plate_kd = gcode.popped_arg();
              }
            }
            break;
          case Gcode::pause:
            // Flush out details about how to resume and what happens to the timer
            // when paused
            break;
          case Gcode::deactivate_all:
            heat_pad_off();
            fan_sink_off();
            peltiers.disable();
            target_temperature_plate = TEMPERATURE_ROOM;
            master_set_a_target = false;
            tc_timer.reset();
            break;
          case Gcode::get_device_info:
            gcode.device_info_response("dummySerial", "dummyModel", device_version);
            break;
          case Gcode::dfu:
            break;
        }
      }
      gcode.send_ack();
    }
  }
#else
void print_info(bool force=false) {
    if (force || (millis() - plotter_timestamp > plotter_interval)) {
        plotter_timestamp = millis();
        if (zoom_mode) {
          // Serial.print(double(millis()) / 1000.0, 2);
          // Serial.print(',');
          Serial.print(current_temperature_plate - target_temperature_plate);
          // Serial.print(',');
          // Serial.print(current_temperature_cover - temperature_cover_hot);
          Serial.println();
          return;
        }
        if (debug_print_mode) Serial.println("\n\n*******");
        if (debug_print_mode) Serial.print("\nAverage Plate-Temp (Heat sink temp):\t\t");
        Serial.print(current_temperature_plate);
        if (debug_print_mode){
          Serial.print("\n\t\t-------- Thermistors -------\n");
          Serial.print(temp_probes.back_left_temperature());
          Serial.print("\t");
          Serial.print(temp_probes.back_center_temperature());
          Serial.print("\t");
          Serial.print(temp_probes.back_right_temperature());
          Serial.print("\n\n");
          Serial.print(temp_probes.front_left_temperature());
          Serial.print("\t");
          Serial.print(temp_probes.front_center_temperature());
          Serial.print("\t");
          Serial.print(temp_probes.front_right_temperature());
          Serial.print("\n");
          Serial.print("\t\t-------------------------------");
        }
        if (running_from_script || running_graph) Serial.print(" ");
        if (debug_print_mode) Serial.print("\n\tPlate-Target:\t");
        if (!master_set_a_target && debug_print_mode) Serial.print("off");
        else Serial.print(target_temperature_plate);
        if (running_from_script || running_graph) Serial.print(" ");
        if (debug_print_mode) Serial.print("\n\tPlate-PID:\t");
        if (!master_set_a_target && debug_print_mode) Serial.print("off");
        else Serial.print((temperature_swing_plate * 50.0) + 50.0);
        if (running_from_script || running_graph) Serial.print(" ");
        if (debug_print_mode) Serial.print("\nCover-Temp:\t\t");
        Serial.print(current_temperature_cover);
        if (running_from_script || running_graph) Serial.print(" ");
        if (debug_print_mode) Serial.print("\n\tCover-Target:\t");
        if (!COVER_SHOULD_BE_HOT && debug_print_mode) Serial.print("off");
        else Serial.print(temperature_cover_hot);
        if (running_from_script || running_graph) Serial.print(" ");
        if (debug_print_mode) Serial.print("\n\tCover-PID:\t");
        if (!COVER_SHOULD_BE_HOT && debug_print_mode) Serial.print("off");
        else Serial.print(temperature_swing_cover * 100.0);
        if (running_from_script || running_graph) Serial.print(" ");
        if (debug_print_mode) Serial.print("\nFan Power:\t\t");
        Serial.print(current_fan_power * 100);
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
            master_set_a_target = false;
            cover_should_be_hot = false;
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
            master_set_a_target = true;
            target_temperature_plate = Serial.parseFloat();
            testing_offset_temp = target_temperature_plate;
            if (Serial.peek() == 'o') {
              Serial.read();
              target_temperature_plate -= Serial.parseFloat();
              Serial.print("Setting Offset to ");
              Serial.println(target_temperature_plate, 6);
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
#endif
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

void setup()
{
  gcode.setup(BAUDRATE);
  delay(1000);
  Serial.println("Setting up stuff..");

  lid.setup();
  peltiers.setup();
  temp_probes.setup(THERMISTOR_VOLTAGE);
  while (!temp_probes.update()) {}
  current_temperature_plate = temp_probes.average_plate_temperature();
  current_temperature_cover = temp_probes.cover_temperature();

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

  target_temperature_plate = TEMPERATURE_ROOM;
  target_temperature_cover = TEMPERATURE_COVER_HOT;
  master_set_a_target = false;
  cover_should_be_hot = false;

  strip.begin();
  strip.setBrightness(32);
  strip.show();
  startTime = micros();  // for the rainbow test
  rainbow_test();
}

void loop()
{
#if USE_GCODES
  read_gcode();
#else
  read_from_serial();
  if (!running_from_script) print_info();
#endif
  if (temp_probes.update())
  {
    current_left_pel_temp = temp_probes.left_pair_temperature();
    current_right_pel_temp = temp_probes.right_pair_temperature();
    current_center_pel_temp = temp_probes.center_pair_temperature();
    current_temperature_plate = temp_probes.average_plate_temperature();
    current_temperature_cover = temp_probes.cover_temperature();
  }
  if (auto_fan)
  {
    update_fan_from_state();
  }
  if (just_changed_temp)
  {
    ramp_temp_after_change_temp();
  }
  if (master_set_a_target && tc_timer.status() == Timer_status::idle && is_at_target())
  {
      tc_timer.start();
  }
  tc_timer.update();
  update_peltiers_from_pid();
  update_cover_from_pid();
}
