#include "thermo-cycler.h"
#include "high_frequency_pwm.h"

TC_Timer tc_timer;
GcodeHandler gcode;
ThermistorsADC temp_probes;
Lid lid;
Peltiers peltiers;
Fan cover_fan;
Fan heatsink_fan;

#if RGBW_NEO
Adafruit_NeoPixel_ZeroDMA strip(NUM_PIXELS, NEO_PIN, NEO_GRBW);
#else
Adafruit_NeoPixel_ZeroDMA strip(NUM_PIXELS, NEO_PIN, NEO_GRB + NEO_KHZ800);
#endif

unsigned long timeStamp = 0;

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
  uint8_t byte_val = max(min(val * 255.0, 255), 0);
#if HFQ_PWM
  hfq_analogWrite(PIN_HEAT_PAD_CONTROL, byte_val);
#else
  analogWrite(PIN_HEAT_PAD_CONTROL, byte_val);
#endif
}

void heat_pad_off()
{
  cover_should_be_hot = false;
  temperature_swing_cover = 0.0;
  target_temperature_cover = TEMPERATURE_ROOM;
#if HW_VERSION >= 3
  digitalWrite(PIN_HEAT_PAD_EN, LOW);
#endif
  set_heat_pad_power(0);
}

void heat_pad_on()
{
  cover_should_be_hot = true;
#if HW_VERSION >= 3
  digitalWrite(PIN_HEAT_PAD_EN, HIGH);
#endif
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void disable_scary_stuff()
{
    heat_pad_off();
    heatsink_fan.disable();
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

void update_fans_from_state()
{
  static unsigned long last_checked = 0;

  if (millis() - last_checked > 100)
  {
    if (!auto_fan)
    { // Heatsink safety threshold overrides manual operation
      if (temp_probes.heat_sink_temperature() > 55)
      {
        // Fan speed proportional to temperature
        float pwr = temp_probes.heat_sink_temperature() / 100;
        pwr = max(pwr, heatsink_fan.manual_power);
        heatsink_fan.set_percentage(pwr);
      }
      return;
    }
    last_checked = millis();
    if (master_set_a_target)
    {
      if (is_target_cold())
      {
        heatsink_fan.set_percentage(0.7);
        return;
      }
      else if (is_ramping_down())
      {
        heatsink_fan.set_percentage(0.55);
        return;
      }
      // else if (is_ramping_up())
      // {
      //   heatsink_fan.set_percentage(0.2);
      //   return;
      // }
      else if (temp_probes.heat_sink_temperature() > 38)
      {
        heatsink_fan.set_percentage(0.2);
      }
    }
    if (temp_probes.heat_sink_temperature() > 55)
    {
      // Fan speed proportional to temperature
      heatsink_fan.set_percentage(temp_probes.heat_sink_temperature() / 100);
    }
    else if(temp_probes.heat_sink_temperature() < 36)
    {
      heatsink_fan.disable();
    }

    // TODO: decide what to do about cover fan
    // if (cover_should_be_hot)
    // {
    //   cover_fan.enable();
    // }
    // else
    // {
    //   cover_fan.disable();
    // }
  }
}

void deactivate_all()
{
  heat_pad_off();
  heatsink_fan.disable();
  peltiers.disable();
  target_temperature_plate = TEMPERATURE_ROOM;
  master_set_a_target = false;
  tc_timer.reset();
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

#if USE_GCODES
  void debug_status_prints()
  {
    static unsigned long lastPrint = 0;

    if (millis() - lastPrint < DEBUG_PRINT_INTERVAL)
    {
      return;
    }
    lastPrint = millis();
    // Timestamp
    gcode.add_debug_timestamp();
    // Target
    gcode.add_debug_response("Plt_Target", target_temperature_plate);
    gcode.add_debug_response("Cov_Target", target_temperature_cover);
    gcode.add_debug_response("Hold_time", tc_timer.time_left());
    // Thermistors:
    gcode.add_debug_response("T1", temp_probes.front_left_temperature());
    gcode.add_debug_response("T2", temp_probes.front_center_temperature());
    gcode.add_debug_response("T3", temp_probes.front_right_temperature());
    gcode.add_debug_response("T4", temp_probes.back_left_temperature());
    gcode.add_debug_response("T5", temp_probes.back_center_temperature());
    gcode.add_debug_response("T6", temp_probes.back_right_temperature());
    gcode.add_debug_response("T.sink", temp_probes.heat_sink_temperature());
    gcode.add_debug_response("loop_time", float(timeStamp)/1000);
    // Fan power:
    gcode.add_debug_response("Fan", heatsink_fan.current_power);
    gcode.add_debug_response("Fan auto?", auto_fan);
    // Cover temperature:
    gcode.add_debug_response("T.Lid", temp_probes.cover_temperature());
    // Motor status:
#if HW_VERSION >= 3
    gcode.add_debug_response("Motor_fault", int(lid.is_driver_faulted()));
#endif
    gcode.response("");
  }

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
          #if LID_TESTING
            gcode_rec_timestamp = millis();
            Serial.print("Opening, ");
            if (lid.open_cover())
            {
              Serial.print("Opened, ");
              Serial.print(millis() - gcode_rec_timestamp);
              Serial.print(", ");
            }
            else
            {
              Serial.print("Errored, ");
              Serial.print(millis() - gcode_rec_timestamp);
              Serial.print(", ");
            }
          #else
            lid.open_cover();
          #endif
            break;
          case Gcode::close_lid:
          #if LID_TESTING
            gcode_rec_timestamp = millis();
            Serial.print("Closing, ");
            if (lid.close_cover())
            {
              Serial.print("Closed, ");
              Serial.println(millis() - gcode_rec_timestamp);
            }
            else
            {
              Serial.print("Errored:");
              Serial.println(millis() - gcode_rec_timestamp);
            }
          #else
            lid.close_cover();
          #endif
            break;
          case Gcode::set_lid_temp:
            if (!gcode.pop_arg('S'))
            {
              target_temperature_cover = TEMPERATURE_COVER_HOT;
            }
            else
            {
              target_temperature_cover = gcode.popped_arg();
            }
            heat_pad_on();
            break;
          case Gcode::get_lid_temp:
            if (cover_should_be_hot)
            {
              gcode.targetting_temperature_response(target_temperature_cover,
              current_temperature_cover);
            }
            else
            {
              gcode.idle_lid_temperature_response(current_temperature_cover);
            }
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
          case Gcode::heatsink_fan_pwr_manual:
            // Control fan power manually. Arg is specified with 'S' and should be
            // a fractional value. If heatsink temperature exceeds safe limit
            // then manual power is overridden by a higher value power proportional
            // to heatsink temperature.
            auto_fan = false;
            heatsink_fan.enable();
            if (gcode.pop_arg('S'))
            {
              heatsink_fan.manual_power = gcode.popped_arg();
              heatsink_fan.set_percentage(heatsink_fan.manual_power);
            }
            break;
          case Gcode::heatsink_fan_auto_on:
            heatsink_fan.manual_power = 0;
            auto_fan = true;
            break;
          case Gcode::pause:
            // Flush out details about how to resume and what happens to the timer
            // when paused
            break;
          case Gcode::deactivate_all:
            deactivate_all();
            break;
          case Gcode::get_device_info:
            gcode.device_info_response(device_serial, device_model, device_version);
            break;
          case Gcode::dfu:
            break;
          case Gcode::debug_mode:
            if (gcode.pop_arg('S'))
            {
              if(gcode.popped_arg() == 0)
              {
                gcode_debug_mode = false;
                break;
              }
            }
            gcode_debug_mode = true;
            break;
          case Gcode::print_debug_stat:
            continuous_debug_stat_mode = false;
            if (gcode_debug_mode)
            {
              debug_status_prints();
            }
            break;
          case Gcode::continous_debug_stat:
            if (gcode_debug_mode)
            {
              continuous_debug_stat_mode = true;
            }
            break;
          case Gcode::motor_reset:
            if (gcode_debug_mode)
            {
              gcode.response("Resetting motor driver");
              lid.reset_motor_driver();
            }
            break;
        }
      }
    #if !LID_TESTING
      if (!continuous_debug_stat_mode)
      {
        gcode.send_ack();
      }
    #endif
    }
    if (continuous_debug_stat_mode)
    {
      debug_status_prints();
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
        if (debug_print_mode) Serial.print("\nHeat sink temp:\t\t");
        Serial.print(temp_probes.heat_sink_temperature());
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
        if (!cover_should_be_hot && debug_print_mode) Serial.print("off");
        else Serial.print(TEMPERATURE_COVER_HOT);
        if (running_from_script || running_graph) Serial.print(" ");
        if (debug_print_mode) Serial.print("\n\tCover-PID:\t");
        if (!cover_should_be_hot && debug_print_mode) Serial.print("off");
        else Serial.print(temperature_swing_cover * 100.0);
        if (running_from_script || running_graph) Serial.print(" ");
        if (debug_print_mode) Serial.print("\nFan Power:\t\t");
        Serial.println(heatsink_fan.current_power * 100);
      #if HW_VERSION >= 3
        Serial.print("Motor faulted?:"); Serial.println(lid.is_driver_faulted());
      #endif
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
              heatsink_fan.set_percentage(percentage / 100.0);
            }
        }
        else if (Serial.peek() == 'p') {
            if (running_from_script) print_info(true);
        }
        else if (Serial.peek() == 'm')
        {
          Serial.println("Resetting motor driver");
          lid.reset_motor_driver();
        }
        else if (Serial.peek() == 'l') // lowercase L
        {
          Serial.read();
          if (Serial.peek() == '1')
          {
            empty_serial_buffer();
            Serial.println("Opening cover");
            lid.open_cover();
          }
          else
          {
            empty_serial_buffer();
            Serial.println("Closing cover");
            lid.close_cover();
          }
        }
        empty_serial_buffer();
    }
}
#endif

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void front_button_callback()
{
  if(digitalRead(PIN_FRONT_BUTTON_SW) == LOW)
  {
    front_button_pressed = true;
    front_button_pressed_at = millis();
  }
}

bool is_front_button_pressed()
{
  if (front_button_pressed && millis() - front_button_pressed_at >= 200)
  {
    if(digitalRead(PIN_FRONT_BUTTON_SW) == LOW)
    {
      front_button_pressed = false;
      return true;
    }
  }
  return false;
}

void set_leds_white()
{
  for(int i=0; i<strip.numPixels(); i++)
  {
  #if RGBW_NEO
    strip.setPixelColor(i, 220, 220, 200, 0); // strip.setPixelColor(n, red, green, blue, white);
  #else
    strip.setPixelColor(i, 220, 220, 200);
  #endif
    strip.show();
    delay(30);
  }
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void setup()
{
  gcode.setup(BAUDRATE);
  delay(1000);
  peltiers.setup();
  temp_probes.setup(THERMISTOR_VOLTAGE);
  while (!temp_probes.update()) {}
  current_temperature_plate = temp_probes.average_plate_temperature();
  current_temperature_cover = temp_probes.cover_temperature();
  if(!lid.setup())
  {
    gcode.response("Lid","I2C Error");
    gcode.send_ack();
  }
  cover_fan.setup_enable_pin(PIN_FAN_COVER, true);  // ON-OFF only. No speed control
  cover_fan.disable();
#if HW_VERSION >=3
  pinMode(PIN_HEAT_PAD_EN, OUTPUT);
  heatsink_fan.setup_enable_pin(PIN_FAN_SINK_ENABLE, true);
#else
  heatsink_fan.setup_enable_pin(PIN_FAN_SINK_ENABLE, false);
#endif
  heatsink_fan.setup_pwm_pin(PIN_FAN_SINK_CTRL);
  heatsink_fan.disable();
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
  target_temperature_cover = TEMPERATURE_ROOM;
  master_set_a_target = false;
  cover_should_be_hot = false;

  pinMode(NEO_PWR, OUTPUT);
  pinMode(NEO_PIN, OUTPUT);
  digitalWrite(NEO_PWR, HIGH);
  strip.begin();
  strip.setBrightness(50);
  strip.show();
  set_leds_white();

#if HW_VERSION >= 3
  pinMode(PIN_FRONT_BUTTON_SW, INPUT);
  analogWrite(PIN_FRONT_BUTTON_LED, LED_BRIGHTNESS);
  attachInterrupt(digitalPinToInterrupt(PIN_FRONT_BUTTON_SW), front_button_callback, CHANGE);
#endif
}

void temp_safety_check()
{
  if (temp_probes.heat_sink_temperature() >= 70
      || (temp_probes.back_left_temperature() > 100
          || temp_probes.back_center_temperature() > 100
          || temp_probes.back_right_temperature() > 100
          || temp_probes.front_left_temperature() > 100
          || temp_probes.front_center_temperature() > 100
          || temp_probes.front_right_temperature() > 100
        )
      )
  {
    gcode.response("System too hot! Deactivating.");
    deactivate_all();
  }
}

void loop()
{
  temp_safety_check();
  lid.check_switches();

#if USE_GCODES
  /* Check if gcode(s) available on Serial */
  read_gcode();
  timeStamp = millis();
  #if LID_WARNING
    if (master_set_a_target && lid.status() != Lid_status::closed)
    {
      gcode.response("WARNING", "Lid Open");
      gcode.send_ack();
    }
  #endif
#else
  read_from_serial();
  if (!running_from_script) print_info();
#endif

#if !DUMMY_BOARD
  if (temp_probes.update())
  {
    current_left_pel_temp = temp_probes.left_pair_temperature();
    current_right_pel_temp = temp_probes.right_pair_temperature();
    current_center_pel_temp = temp_probes.center_pair_temperature();
    current_temperature_plate = temp_probes.average_plate_temperature();
    current_temperature_cover = temp_probes.cover_temperature();
  }
#endif

  update_fans_from_state();

  if (master_set_a_target && tc_timer.status() == Timer_status::idle && is_at_target())
  {
    tc_timer.start();
  }
  tc_timer.update();
  update_peltiers_from_pid();
  update_cover_from_pid();
#if HW_VERSION >=3
  if (is_front_button_pressed())
  {
    deactivate_all();
    lid.open_cover();
  }
#endif
  timeStamp = millis() - timeStamp;
}
