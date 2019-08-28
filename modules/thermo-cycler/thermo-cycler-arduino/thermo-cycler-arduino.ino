#include "thermo-cycler.h"
#include "high_frequency_pwm.h"

TC_Timer tc_timer;
GcodeHandler gcode;
ThermistorsADC temp_probes;
Lid lid;
Lights light_strip;
Peltiers peltiers;
Fan cover_fan;
Fan heatsink_fan;


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

void update_peltiers_from_pid(Peltier pel_n)
{
  if (master_set_a_target)
  {
    switch(pel_n)
    {
      case Peltier::pel_1:
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
        break;
      case Peltier::pel_2:
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
        break;
      case Peltier::pel_3:
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
        break;
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
      if (temp_probes.heat_sink_temperature() > HEATSINK_FAN_HI_TEMP)
      {
        // Fan speed proportional to temperature
        float pwr = HEATSINK_P_CONSTANT * temp_probes.heat_sink_temperature() / 100.0;
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
        heatsink_fan.set_percentage(FAN_PWR_COLD_TARGET);
        return;
      }
      else if (is_ramping_down())
      {
        heatsink_fan.set_percentage(FAN_PWR_RAMPING_DOWN);
        return;
      }
      // else if (is_ramping_up())
      // {
      //   heatsink_fan.set_percentage(0.2);
      //   return;
      // }
      else if (temp_probes.heat_sink_temperature() > HEATSINK_FAN_LO_TEMP)
      {
        heatsink_fan.set_percentage(FAN_POWER_LOW);
      }
    }
    if (temp_probes.heat_sink_temperature() > HEATSINK_FAN_HI_TEMP)
    {
      // Fan speed proportional to temperature
      heatsink_fan.set_percentage(HEATSINK_P_CONSTANT * temp_probes.heat_sink_temperature() / 100.0);
    }
    else if(temp_probes.heat_sink_temperature() < HEATSINK_FAN_OFF_TEMP)
    {
      heatsink_fan.disable();
    }
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
  // Thermistor status:
  gcode.add_debug_response("T_error", int(temp_probes.detected_invalid_val));
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
          lid.open_cover();
          Serial.print(lid.LID_STATUS_STRINGS[static_cast<int>(lid.status())]);
          Serial.print(", ");
          Serial.print(millis() - gcode_rec_timestamp);
          Serial.print(", ");
        #else
          lid.open_cover();
        #endif
          break;
        case Gcode::close_lid:
        #if LID_TESTING
          gcode_rec_timestamp = millis();
          Serial.print("Closing, ");
          lid.close_cover();
          Serial.print(lid.LID_STATUS_STRINGS[static_cast<int>(lid.status())]);
          Serial.print(", ");
          Serial.println(millis() - gcode_rec_timestamp);
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
        case Gcode::set_led:
          if (gcode.pop_arg('C'))
          {
            light_strip.api_color = static_cast<Light_color>(gcode.popped_arg());
          }
          if (gcode.pop_arg('A'))
          {
            light_strip.api_action = static_cast<Light_action>(gcode.popped_arg());
          }
          break;
        case Gcode::set_led_override:
          if (gcode.pop_arg('C'))
          {
            if (gcode.popped_arg() == 1)
            {
              light_strip.color_override = true;
            }
            else if (gcode.popped_arg() == 0)
            {
              light_strip.color_override = false;
            }
          }
          if (gcode.pop_arg('A'))
          {
            if (gcode.popped_arg() == 1)
            {
              light_strip.action_override = true;
            }
            else if (gcode.popped_arg() == 0)
            {
              light_strip.action_override = false;
            }
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

void update_light_strip()
{
  if (master_set_a_target)
  {
    if (is_target_hot())
    {
      if (is_at_target())
      {
        light_strip.set_lights(TC_status::at_hot_target);
      }
      else
      {
        light_strip.set_lights(TC_status::going_to_hot_target);
      }
    }
    else
    {
      if (is_at_target())
      {
        light_strip.set_lights(TC_status::at_cold_target);
      }
      else
      {
        light_strip.set_lights(TC_status::going_to_cold_target);
      }
    }
  }
  else
  {
    light_strip.set_lights(TC_status::idle);
  }
  light_strip.update();
}

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
#if HW_VERSION >=3
  if (front_button_pressed && millis() - front_button_pressed_at >= 200)
  {
    if(digitalRead(PIN_FRONT_BUTTON_SW) == LOW)
    {
      front_button_pressed = false;
      return true;
    }
  }
#endif
  return false;
}

void set_25ms_interrupt()
{
  GCLK->GENDIV.reg = GCLK_GENDIV_DIV(200) | GCLK_GENDIV_ID(4);  // 8M/200 = 4M
  while ( GCLK->STATUS.reg & GCLK_STATUS_SYNCBUSY );  /* Wait for synchronization */

  GCLK->GENCTRL.reg = GCLK_GENCTRL_IDC |           // Set duty cycle to 50%
                      GCLK_GENCTRL_GENEN |         // Enable GCLK4
                      GCLK_GENCTRL_SRC_OSC8M |     // Select 8MHz clock source
                      GCLK_GENDIV_ID(4);           // Select GCLK4
  while ( GCLK->STATUS.reg & GCLK_STATUS_SYNCBUSY ); /* Wait for synchronization */

  GCLK->CLKCTRL.reg = (uint16_t) (GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK4 | GCLK_CLKCTRL_ID(GCM_TC4_TC5));
  while (GCLK->STATUS.bit.SYNCBUSY == 1);

  // -- Configure TC
  // Disable TC
  TC4->COUNT8.CTRLA.bit.ENABLE = 0;
  while (TC4->COUNT16.STATUS.bit.SYNCBUSY);

  // Set Timer counter Mode to 8 bits, normal PWM
  TC4->COUNT8.CTRLA.reg |= TC_CTRLA_MODE_COUNT8 | TC_CTRLA_WAVEGEN_NPWM;
  while (TC4->COUNT8.STATUS.bit.SYNCBUSY);

  // Set PER to calculated value
  // 0xFA = 250 // @f=10kHz, timer interval = 1/10kHz * 250 = 25ms
  TC4->COUNT8.PER.reg = 0xFA;
  while (TC4->COUNT8.STATUS.bit.SYNCBUSY);

  NVIC_SetPriority(TC4_IRQn, 0);    // Set NVIC priority for TC4 to 0 (highest)
  NVIC_EnableIRQ(TC4_IRQn);         // Connect TC4 to NVIC

  TC4->COUNT8.INTFLAG.reg |= TC_INTFLAG_MC1 | TC_INTFLAG_MC0 | TC_INTFLAG_OVF;  // Clear interrupt flags
  TC4->COUNT8.INTENSET.reg |= TC_INTENSET_OVF; // Enable TC4 ovf interrupt

  // Set prescaler to 4, So f=10kHz (legal values 1,2,4,8,16,64,256,1024)
  TC4->COUNT8.CTRLA.reg |= TC_CTRLA_PRESCALER_DIV4 | TC_CTRLA_PRESCSYNC_PRESC;
  while (TC4->COUNT8.STATUS.bit.SYNCBUSY);      // Wait for synchronization

  // Enable TC
  TC4->COUNT8.CTRLA.bit.ENABLE = 1;
  while (TC4->COUNT8.STATUS.bit.SYNCBUSY);
}

void TC4_Handler()
{
  if (TC4->COUNT8.INTFLAG.bit.OVF && TC4->COUNT8.INTENSET.bit.OVF)
  {
    timer_interrupted = true;
    if (therm_read_state >= 4)
    {
      therm_read_state = 1;
    }
    else
    {
      therm_read_state += 1;
    }
    TC4->COUNT8.INTFLAG.bit.MC0 = 1;
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
  temp_probes.update(ThermistorPair::right);
  temp_probes.update(ThermistorPair::center);
  temp_probes.update(ThermistorPair::left);
  temp_probes.update(ThermistorPair::cover_n_heatsink);

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

  PID_left_pel.SetSamplingMode(MANUAL);
  PID_left_pel.SetTunings(current_plate_kp, current_plate_ki, current_plate_kd, P_ON_M);
  PID_left_pel.SetMode(AUTOMATIC);
  PID_left_pel.SetOutputLimits(-1.0, 1.0);
  PID_center_pel.SetSamplingMode(MANUAL);
  PID_center_pel.SetTunings(current_plate_kp, current_plate_ki, current_plate_kd, P_ON_M);
  PID_center_pel.SetMode(AUTOMATIC);
  PID_center_pel.SetOutputLimits(-1.0, 1.0);
  PID_right_pel.SetSamplingMode(MANUAL);
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

  light_strip.setup();

#if HW_VERSION >= 3
  pinMode(PIN_FRONT_BUTTON_SW, INPUT);
  analogWrite(PIN_FRONT_BUTTON_LED, LED_BRIGHTNESS);
  attachInterrupt(digitalPinToInterrupt(PIN_FRONT_BUTTON_SW), front_button_callback, CHANGE);
#endif
  set_25ms_interrupt();
}

void temp_safety_check()
{
  if (!master_set_a_target)
  {
    return;
  }
  if (temp_probes.heat_sink_temperature() >= HEATSINK_SAFE_TEMP_LIMIT
      || (temp_probes.back_left_temperature() > PELTIER_SAFE_TEMP_LIMIT
          || temp_probes.back_center_temperature() > PELTIER_SAFE_TEMP_LIMIT
          || temp_probes.back_right_temperature() > PELTIER_SAFE_TEMP_LIMIT
          || temp_probes.front_left_temperature() > PELTIER_SAFE_TEMP_LIMIT
          || temp_probes.front_center_temperature() > PELTIER_SAFE_TEMP_LIMIT
          || temp_probes.front_right_temperature() > PELTIER_SAFE_TEMP_LIMIT
        )
      )
  {
    gcode.response("System too hot! Deactivating.");
    deactivate_all();
  }
}

void temp_plot()
{
  static unsigned long lastPrint = 0;

  if (millis() - lastPrint < 150)
  {
    return;
  }
  lastPrint = millis();
  Serial.print(target_temperature_plate); Serial.print("\t");
  Serial.print(temp_probes.front_left_temperature()); Serial.print("\t");
  Serial.print(temp_probes.front_center_temperature()); Serial.print("\t");
  Serial.print(temp_probes.front_right_temperature()); Serial.print("\t");
  Serial.print(temp_probes.back_left_temperature()); Serial.print("\t");
  Serial.print(temp_probes.back_center_temperature()); Serial.print("\t");
  Serial.print(temp_probes.back_right_temperature()); Serial.print("\t");
  Serial.print(temp_probes.heat_sink_temperature()); Serial.print("\t");
  Serial.print(temp_probes.cover_temperature()); Serial.print("\t");
  Serial.println();
}

/* **** therm_pid_peltier_update ******
 * This function would run every 25ms (timed by an interrupt) and will:
 * 1. Read the values of a specific pair of thermistors on each iteration &
 * compute PID & update peltiers in the following manner:
 * 1st interrupt- read pel_1 thermistors, compute pel_1 PID, update pel_1
 * 2nd interrupt- read pel_2 thermistors, compute pel_2 PID, update pel_2
 * 3rd interrupt- read pel_3 thermistors, compute pel_3 PID, update pel_3
 * 4th interrupt- read heatsink & cover thermistors
 * ..repeat all 4 interrupts..
 * This makes sure all three PID computes use the very latest thermistor reading
 * available and the computation happens exactly every 100ms
 */
void therm_pid_peltier_update()
{
  if (timer_interrupted)
  {
    switch(therm_read_state)
    {
      case 1:
        temp_probes.update(ThermistorPair::right);
        current_right_pel_temp = temp_probes.right_pair_temperature();
        update_peltiers_from_pid(Peltier::pel_1);
        break;
      case 2:
        temp_probes.update(ThermistorPair::center);
        current_center_pel_temp = temp_probes.center_pair_temperature();
        update_peltiers_from_pid(Peltier::pel_2);
        break;
      case 3:
        temp_probes.update(ThermistorPair::left);
        current_left_pel_temp = temp_probes.left_pair_temperature();
        update_peltiers_from_pid(Peltier::pel_3);
        break;
      case 4:
        temp_probes.update(ThermistorPair::cover_n_heatsink);
        current_temperature_cover = temp_probes.cover_temperature();
        break;
    }
    current_temperature_plate = temp_probes.average_plate_temperature();
    timer_interrupted = false;
  }
}

void loop()
{
  timeStamp = micros();
  therm_pid_peltier_update();
  if (temp_probes.detected_invalid_val)
  {
    if (millis() - last_error_print > ERROR_PRINT_INTERVAL)
    {
      gcode.response("ERROR", "Invalid thermistor value");
      last_error_print = millis();
    }
    deactivate_all();
  }
  temp_safety_check();
  // temp_plot();
  lid.check_switches();
  #if LID_WARNING
  // TODO: Confirm if lid warning is required at all
    if (master_set_a_target && lid.status() != Lid_status::closed)
    {
      if (millis() - last_error_print > ERROR_PRINT_INTERVAL)
      {
        gcode.response("WARNING", "Lid Open");
        last_error_print = millis();
      }
    }
  #endif

  update_fans_from_state();

  if (master_set_a_target && tc_timer.status() == Timer_status::idle && is_at_target())
  {
    tc_timer.start();
  }
  tc_timer.update();
  update_cover_from_pid();
  if (is_front_button_pressed())
  {
    if(lid.status() == Lid_status::closed || lid.status() == Lid_status::in_between)
    {
      lid.open_cover();
    }
    else if (lid.status() == Lid_status::open)
    {
      lid.close_cover();
    }
    else
    {
      // Else, lid status is unknown and there could be issues with lid switches.
      // Hence, an open/close won't be performed
      gcode.response("Error", "Lid_status: unknown");
    }
  }
  timeStamp = micros() - timeStamp;
  /* Check if gcode(s) available on Serial */
  read_gcode();
  update_light_strip();
}
