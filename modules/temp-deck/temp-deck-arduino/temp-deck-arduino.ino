/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

#include "temp-deck.h"

PID myPID(
  &CURRENT_TEMPERATURE,
  &TEMPERATURE_SWING,
  &TARGET_TEMPERATURE,
  DOWN_PID_KP, DOWN_PID_KI, DEFAULT_PID_KD,
  P_ON_M,
  DIRECT);

String device_serial;  // this value is read from eeprom during setup()
String device_model;   // this value is read from eeprom during setup()

Lights lights = Lights();  // controls 2-digit 7-segment numbers, and the RGBW color bar
Peltiers peltiers = Peltiers();  // 2 peltiers wired in series (-1.0<->1.0 controls polarity and intensity)
Thermistor thermistor = Thermistor();  // uses thermistor to read calculate the top-plate's temperature
Gcode gcode = Gcode();  // reads in serial data to parse command and issue reponses
Memory memory = Memory();  // reads from EEPROM to find device's unique serial, and model number

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

bool is_stabilizing() {
  return abs(TARGET_TEMPERATURE - CURRENT_TEMPERATURE) <= STABILIZING_ZONE;
}

bool is_moving_down() {
  return CURRENT_TEMPERATURE - TARGET_TEMPERATURE > STABILIZING_ZONE;
}

bool is_moving_up() {
  return TARGET_TEMPERATURE - CURRENT_TEMPERATURE > STABILIZING_ZONE;
}

bool is_burning_hot() {
  return CURRENT_TEMPERATURE > TEMPERATURE_BURN;
}

bool is_cold_zone() {
  return CURRENT_TEMPERATURE <= TEMPERATURE_FAN_CUTOFF_COLD;
}

bool is_middle_zone() {
  return CURRENT_TEMPERATURE > TEMPERATURE_FAN_CUTOFF_COLD && CURRENT_TEMPERATURE <= TEMPERATURE_FAN_CUTOFF_HOT;
}

bool is_hot_zone() {
  return CURRENT_TEMPERATURE > TEMPERATURE_FAN_CUTOFF_HOT;
}

bool is_targeting_cold_zone() {
  return TARGET_TEMPERATURE <= TEMPERATURE_FAN_CUTOFF_COLD;
}

bool is_targeting_middle_zone() {
  return TARGET_TEMPERATURE > TEMPERATURE_FAN_CUTOFF_COLD && TARGET_TEMPERATURE <= TEMPERATURE_FAN_CUTOFF_HOT;
}

bool is_targeting_hot_zone() {
  return TARGET_TEMPERATURE > TEMPERATURE_FAN_CUTOFF_HOT;
}

bool is_fan_on_high() {
  return is_targeting_cold_zone() || is_moving_down();
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void set_target_temperature(double target_temp){
  if (target_temp < TEMPERATURE_MIN) {
    target_temp = TEMPERATURE_MIN;
    gcode.print_warning(
      F("Target temperature too low, setting to TEMPERATURE_MIN degrees"));
  }
  if (target_temp > TEMPERATURE_MAX) {
    target_temp = TEMPERATURE_MAX;
    gcode.print_warning(
      F("Target temperature too high, setting to TEMPERATURE_MAX degrees"));
  }

#ifdef CONSERVE_POWER_ON_SET_TARGET
  // in case the fan was PREVIOUSLY set on high
  if (is_fan_on_high()) {
    SET_TEMPERATURE_TIMESTAMP = millis();
  }
#endif

  TARGET_TEMPERATURE = target_temp;  // set the target temperature

#ifdef CONSERVE_POWER_ON_SET_TARGET
  // in case the fan is NOW GOING TO be set on high (with new target)
  if (is_fan_on_high()) {
    SET_TEMPERATURE_TIMESTAMP = millis();
  }
#endif

  lights.flash_on();

  adjust_pid_on_new_target();
}

void turn_off_target() {
  set_target_temperature(TEMPERATURE_ROOM);
  MASTER_SET_A_TARGET = false;
  lights.flash_off();
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void set_fan_power(float percentage){
  percentage = constrain(percentage, 0.0, 1.0);
  if (is_v3_v4_fan) {
    fan_on_time = percentage * MAX_FAN_OFF_TIME;
    fan_off_time = MAX_FAN_OFF_TIME - fan_on_time;
  }
  else {
    analogWrite(PIN_FAN, int(percentage * 255.0));
  }
}

void fan_v3_v4_on() {
  if (is_fan_on_high())
  {
    analogWrite(PIN_FAN, FAN_V3_V4_HI_PWR);
  }
  else
  { // If it's a PWM fan, it'll be powered down to the low power value.
    // If it's not a PWM fan, low pwm will still turn its driver ON as long as
    // it's above a threshold.
    analogWrite(PIN_FAN, FAN_V3_V4_LOW_PWR);
  }
  is_fan_on = true;
}

void fan_v3_v4_off() {
  analogWrite(PIN_FAN, LOW);
  is_fan_on = false;
}

void adjust_v3_v4_fan_state() {
  if (fan_on_time == 0) fan_v3_v4_off();
  else if (fan_off_time == 0) fan_v3_v4_on();
  else {
    if (is_fan_on) {
      if (millis() - fan_timestamp > fan_on_time) {
        fan_timestamp = millis();
        fan_v3_v4_off();
      }
    }
    else if (millis() - fan_timestamp > fan_off_time) {
      fan_timestamp = millis();
      fan_v3_v4_on();
    }
  }
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void set_peltiers_from_pid() {
  if (TEMPERATURE_SWING < 0) {
    peltiers.set_cold_percentage(abs(TEMPERATURE_SWING));
  }
  else {
    peltiers.set_hot_percentage(TEMPERATURE_SWING);
  }
}

void adjust_pid_on_new_target() {
  if (is_moving_up()) {
    if (is_targeting_cold_zone()) {
      myPID.SetTunings(
        UP_PID_KP_IN_COLD_ZONE,
        UP_PID_KI_IN_COLD_ZONE,
        DEFAULT_PID_KD,
        P_ON_M);
    }
    else if (TARGET_TEMPERATURE <= UP_PID_LOW_TEMP) {
      myPID.SetTunings(
        UP_PID_KP_AT_LOW_TEMP,
        UP_PID_KI_AT_LOW_TEMP,
        DEFAULT_PID_KD,
        P_ON_M);
    }
    else if (TARGET_TEMPERATURE >= UP_PID_HIGH_TEMP) {
      myPID.SetTunings(
        UP_PID_KP_AT_HIGH_TEMP,
        UP_PID_KI_AT_HIGH_TEMP,
        DEFAULT_PID_KD,
        P_ON_M);
    }
    else {
      // linear function between the MIN and MAX Kp and Ki values when moving up
      float scaler = (TARGET_TEMPERATURE - UP_PID_LOW_TEMP) / (UP_PID_HIGH_TEMP - UP_PID_LOW_TEMP);
      myPID.SetTunings(
        UP_PID_KP_AT_LOW_TEMP + (scaler * (UP_PID_KP_AT_HIGH_TEMP - UP_PID_KP_AT_LOW_TEMP)),
        UP_PID_KI_AT_LOW_TEMP + (scaler * (UP_PID_KI_AT_HIGH_TEMP - UP_PID_KI_AT_LOW_TEMP)),
        DEFAULT_PID_KD,
        P_ON_M);
    }
  }
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void stabilize_to_target_temp(bool set_fan=true){

#ifdef CONSERVE_POWER_ON_SET_TARGET
  // first, avoid drawing too much current when the target was just changed
  unsigned long now = millis();
  if (SET_TEMPERATURE_TIMESTAMP > now) SET_TEMPERATURE_TIMESTAMP = now;  // handle rollover
  unsigned long end_time = SET_TEMPERATURE_TIMESTAMP + millis_till_fan_turns_off;
  if (end_time > now) {
    peltiers.disable_peltiers();
    set_fan_power(FAN_OFF);
    return;  // EXIT the function, do nothing, wait
  }
  end_time += millis_till_peltiers_drop_current;
  if (end_time > now) {
    set_fan = false;
  }
#endif

  // seconds, set the fan to the correct intensity
  if (set_fan == false) {
    set_fan_power(FAN_OFF);
  }
  else if (is_fan_on_high()) {
    set_fan_power(FAN_HIGH);
  }
  else {
    if (is_v3_v4_fan) set_fan_power(FAN_V3_V4_LOW_ON_PC); // more like set fan ON time
    else set_fan_power(FAN_LOW);
  }

  // third, update the
  set_peltiers_from_pid();
}

void stabilize_to_room_temp(bool set_fan=true) {

  if (is_burning_hot && reached_unsafe_temp)
  { // the peltiers have probably run away so disable them
    peltiers.disable_peltiers();
    set_fan_power(FAN_HIGH);
  }
  else if (is_burning_hot()) {
    // turn ON peltiers to cool to < TEMPERATURE_BURN
    set_peltiers_from_pid();
    set_fan_power(FAN_HIGH);
  }
  else if (!is_burning_hot()) {
    peltiers.disable_peltiers();
    set_fan_power(FAN_OFF);
  }
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void update_led_display(boolean debounce=true){
  // round to closest whole-number temperature
  lights.display_number(CURRENT_TEMPERATURE + 0.5, debounce);
  // if we are targetting, and close to the value, stop flashing
  if (!MASTER_SET_A_TARGET || is_stabilizing()) {
    lights.flash_off();
  }
  // targetting and not close to value, continue flashing
  else {
    lights.flash_on();
  }

  // set the color-bar color depending on if we're targing hot or cold temperatures
  if (!MASTER_SET_A_TARGET) {
    lights.set_color_bar(0, 0, 0, 1);  // white
  }
  else if (TARGET_TEMPERATURE <= TEMPERATURE_ROOM) {
    lights.set_color_bar(0, 0, 1, 0);  // blue
  }
  else {
    lights.set_color_bar(1, 0, 0, 0);  // red
  }
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void read_thermistor_and_apply_offset() {
  if (thermistor.update()) {
    CURRENT_TEMPERATURE = thermistor.temperature();
    // apply a small offset to the temperature
    // depending on how far below/above room temperature we currently are
    _offset_temp_diff = CURRENT_TEMPERATURE - TEMPERATURE_ROOM;
    if (_offset_temp_diff > 0) {
      CURRENT_TEMPERATURE += (_offset_temp_diff / THERMISTOR_OFFSET_HIGH_TEMP_DIFF) * THERMISTOR_OFFSET_HIGH_VALUE;
    }
    else {
      CURRENT_TEMPERATURE += (abs(_offset_temp_diff) / THERMISTOR_OFFSET_LOW_TEMP_DIFF) * THERMISTOR_OFFSET_LOW_VALUE;
    }
  }
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void activate_bootloader(){
  // Method 1: Uses a WDT reset to enter bootloader.
  // Works on the modified Caterina bootloader that allows
  // bootloader access after a WDT reset
  // -----------------------------------------------------------------
  unsigned long tempWait = millis();
  while(millis() - tempWait < 3000){
    // Pi has 3 seconds to cleanly disconnect
  }
  wdt_enable(WDTO_15MS);  //Timeout
  unsigned long timerStart = millis();
  while(millis() - timerStart < 25){
    //Wait out until WD times out
  }
  // Should never get here but in case it does because
  // WDT failed to start or timeout..
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void print_temperature() {
  if (MASTER_SET_A_TARGET) {
    gcode.print_targetting_temperature(
      TARGET_TEMPERATURE,
      CURRENT_TEMPERATURE
    );
  }
  else {
    gcode.print_stablizing_temperature(
      CURRENT_TEMPERATURE
    );
  }
}

void read_gcode(){
  if (gcode.received_newline()) {
    while (gcode.pop_command()) {
      switch(gcode.code) {
        case GCODE_NO_CODE:
          break;
        case GCODE_GET_TEMP:
          print_temperature();
          break;
        case GCODE_SET_TEMP:
          if (gcode.read_number('S')) {
            set_target_temperature(gcode.parsed_number);
            MASTER_SET_A_TARGET = true;
          }
          break;
        case GCODE_DISENGAGE:
          turn_off_target();
          break;
        case GCODE_DEVICE_INFO:
          gcode.print_device_info(device_serial, device_model, FW_VERSION);
          break;
        case GCODE_DFU:
          gcode.send_ack(); // Send ack here since we not reaching the end of the loop
          gcode.print_warning(F("Restarting and entering bootloader..."));
          activate_bootloader();
          break;
        default:
          break;
      }
    }
    gcode.send_ack();
  }
}

void turn_off_serial_lights() {
  // disable the Tx/Rx LED output (too distracting)
  TXLED1;
  RXLED1;
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void setup() {

  turn_off_serial_lights();

  wdt_disable();          /* Disable watchdog if enabled by bootloader/fuses */

  gcode.setup(115200);

  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_FAN, OUTPUT);

  set_fan_power(FAN_OFF);

  // 250ms is the PWM cycle-time (up/down) for the peltiers
  // it shouldn't be shorter than 100ms, as peltiers are not designed to cycle that fast
  peltiers.setup_peltiers(250);
  peltiers.disable_peltiers();

  memory.read_serial(device_serial);
  memory.read_model(device_model);
  String ver = device_model.substring(MODEL_VER_TEMPLATE_LEN);
  int model_version = ver.toInt();

  // LED pins for model versions 3 (post 2018.10.15) & 4: red = 6, blue = 5
  // LED pins for model versions < 3 & 3.0 (pre- 2018.10.15) : red = 5, blue = 6
  bool is_blue_pin_5;
  if (model_version >= 3)
  {
    if (model_version == 3)
    {
      is_v3_v4_fan = true;
      // V3 tempdecks produced after Oct 15 have different LED pin configuration
      // serial number has the production date. eg. TDV03P*20181008*A01
      const uint8_t date_length = 4;  // MMDD
      String serial_date = device_serial.substring(
        SERIAL_VER_TEMPLATE_LEN, SERIAL_VER_TEMPLATE_LEN + date_length);
      int v3_date = serial_date.toInt();
      if (v3_date > 1015)
      {
        is_blue_pin_5 = true;
      }
      else
      {
        is_blue_pin_5 = false;
      }
    }
    else if (model_version == 4)
    {
      is_blue_pin_5 = true;
      is_v3_v4_fan = true;
    }
    else
    {
      is_blue_pin_5 = true;
      is_v3_v4_fan = false;
    }
  }
  else
  {
    is_v3_v4_fan = false;
    is_blue_pin_5 = false;
  }
  lights.setup_lights(is_blue_pin_5);
  lights.set_numbers_brightness(0.25);
  lights.set_color_bar_brightness(0.5);

  // make sure we start with an averaged plate temperatures
  while (!thermistor.update()) {}
  CURRENT_TEMPERATURE = thermistor.temperature();
  TARGET_TEMPERATURE = TEMPERATURE_ROOM;

  // setup PID
  myPID.SetMode(AUTOMATIC);
  myPID.SetSampleTime(500);  // since peltier's update at 250ms, PID can be slower
  myPID.SetOutputLimits(-1.0, 1.0);
  // make sure we start with a calculated PID output
  while (!myPID.Compute()) {}

  turn_off_target();
  lights.startup_animation(CURRENT_TEMPERATURE, 2000);
}

void temp_safety_check()
{
  if (CURRENT_TEMPERATURE > TEMPERATURE_MAX)
  { // Once this boolean is set, it will not reset unless device is power cycled
    reached_unsafe_temp = true;
  }
}

void loop(){
  temp_safety_check();
  static unsigned long last_print = millis();
  if (millis() - last_print >= ERROR_PRINT_INTERVAL)
  {
    last_print = millis();
    if (reached_unsafe_temp)
    {
      gcode.print_warning(F("Temperature module overheated! Deactivating."));
      turn_off_target();
      peltiers.disable_peltiers();
      set_fan_power(FAN_HIGH);
    }
  }
  turn_off_serial_lights();

#ifdef DEBUG_PLOTTER_ENABLED
  if (debug_plotter_timestamp + DEBUG_PLOTTER_INTERVAL < millis()) {
    debug_plotter_timestamp = millis();
    Serial.print(TARGET_TEMPERATURE);
    Serial.print('\t');
    Serial.print(CURRENT_TEMPERATURE);
    Serial.print('\t');
    Serial.println((TEMPERATURE_SWING * 50) + 50.0);
  }
#endif

  // update the peltiers' ON/OFF cycle
  peltiers.update_peltier_cycle();

  read_gcode();

  read_thermistor_and_apply_offset();

  if (is_v3_v4_fan) adjust_v3_v4_fan_state();

  // update the temperature display, and color-bar
  update_led_display(true);  // debounce enabled

  if (myPID.Compute()) {  // Compute() should run every loop
    if (MASTER_SET_A_TARGET) {
      stabilize_to_target_temp();
    }
    else {
      stabilize_to_room_temp();
    }
  }
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////
