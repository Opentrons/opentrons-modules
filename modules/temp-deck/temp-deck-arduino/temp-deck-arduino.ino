/////////////////////////////////
/////////////////////////////////
///////////////////////////////////

#include <Arduino.h>
#include <Adafruit_PWMServoDriver.h>

// called this way, it uses the default address 0x40
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();
// you can also call it with a different address you want
//Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x41);
// you can also call it with a different address and I2C interface
//Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(&Wire, 0x40);

#define thermistor 5
#define on_board_thermistor 4

#define red_led 5
#define blue_led 6

#define peltier_a_control 13
#define peltier_b_control 10
#define peltier_ab_enable 8

#define tone_out 11

#define fan_pwm 9

const int TABLE_SIZE = 21;
const unsigned int TABLE[TABLE_SIZE][3] = {  // ADC, Celsius, Ohms
  {781, 0, 32330},
  {732, 5, 25194},
  {680, 10, 19785},
  {624, 15, 15651},
  {568, 20, 12468},
  {512, 25, 10000},
  {457, 30, 8072},
  {405, 35, 6556},
  {357, 40, 5356},
  {313, 45, 4401},
  {273, 50, 3635},
  {237, 55, 3019},
  {206, 60, 2521},
  {179, 65, 2115},
  {155, 70, 1781},
  {134, 75, 1509},
  {116, 80, 1284},
  {101, 85, 1097},
  {88, 90, 941},
  {77, 95, 810},
  {67, 100, 701}
};

const uint8_t letters[2][7] = {
  {10, 11, 4, 5, 6, 9, 8},
  {14, 15, 1, 2, 3, 13, 12}
};

// arrays of on/off values for each number character 0-9 on the 7-Segment displays
const uint8_t numbers[10][7] = {
  {1, 1, 1, 1, 1, 1, 0},  // 0
  {0, 1, 1, 0, 0, 0, 0},  // 1
  {1, 1, 0, 1, 1, 0, 1},  // 2
  {1, 1, 1, 1, 0, 0, 1},  // 3
  {0, 1, 1, 0, 0, 1, 1},  // 4
  {1, 0, 1, 1, 0, 1, 1},  // 5
  {1, 0, 1, 1, 1, 1, 1},  // 6
  {1, 1, 1, 0, 0, 0, 0},  // 7
  {1, 1, 1, 1, 1, 1, 1},  // 8
  {1, 1, 1, 1, 0, 1, 1}   // 9
};

const uint8_t message_low[2][7] = {
  {0, 0, 0, 1, 1, 1, 0},  // L
  {1, 1, 1, 1, 1, 1, 0}   // O
};

const uint8_t message_high[2][7] = {
  {0, 1, 1, 0, 1, 1, 1},  // H
  {0, 1, 1, 0, 0, 0, 0}   // I
};

const uint8_t green_pwm_pin = 0;
const uint8_t white_pwm_pin = 7;

int prev_temp = 0;

int TARGET_TEMPERATURE = 25;
boolean IS_TARGETING = false;

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void setup_pins(){
  pinMode(red_led, OUTPUT);
  pinMode(blue_led, OUTPUT);
  pinMode(peltier_a_control, OUTPUT);
  pinMode(peltier_b_control, OUTPUT);
  pinMode(peltier_ab_enable, OUTPUT);
  pinMode(tone_out, OUTPUT);
  pinMode(fan_pwm, OUTPUT);
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void setup_lights() {
  pwm.begin();
  pwm.setPWMFreq(1600);  // This is the maximum PWM frequency

  clear_display();
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void startup_color_animation() {
  display_number(88);
  set_color_bar(0, 0, 0, 1);  // white (rgbw)
  delay(200);
  set_color_bar(0, 0, 1, 0);  // white (rgbw)
  delay(200);
  set_color_bar(0, 1, 0, 0);  // white (rgbw)
  delay(200);
  set_color_bar(1, 0, 0, 0);  // white (rgbw)
  delay(200);
  set_color_bar(0, 0, 0, 1);
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void display_number(int number) {
  if (number > 99){
    show_message_low();
    return;
  }
  else if (number < 0){
    show_message_high();
    return;
  }
  uint8_t first_character = number / 10;
  uint8_t second_character = number % 10;
  clear_display();
  for (uint8_t i=0;i<7;i++){
    set_pwm_pin(letters[0][i], numbers[first_character][i]);
    set_pwm_pin(letters[1][i], numbers[second_character][i]);
  }
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void show_message_low(){
  clear_display();
  for (uint8_t i=0;i<7;i++){
    set_pwm_pin(letters[0][i], message_low[0][i]);
    set_pwm_pin(letters[1][i], message_low[1][i]);
  }
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void show_message_high(){
  clear_display();
  for (uint8_t i=0;i<7;i++){
    set_pwm_pin(letters[0][i], message_high[0][i]);
    set_pwm_pin(letters[1][i], message_high[1][i]);
  }
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void set_target_temperature(int target_temp){
  if (target_temp < 0) target_temp = 0;
  if (target_temp > 99) target_temp = 99;
  IS_TARGETING = true;
  TARGET_TEMPERATURE = target_temp;
}

void disable_target(){
  IS_TARGETING = false;
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void delay_minutes(int minutes){
  for (int i=0;i<minutes;i++){
    for (int s=0; s<60; s++){
      delay(1000);  // 1 minute
    }
  }
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void set_cold_percentage(float perc){
  set_peltiers_percentage(perc, 0);
  set_color_bar(0, 0, 1, 0);
}

void set_hot_percentage(float perc){
  set_peltiers_percentage(0, perc);
  set_color_bar(1, 0, 0, 0);
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void clear_display(){
  for (uint8_t l=0; l < 7; l++) {
    set_pwm_pin(letters[0][l], 0);
    set_pwm_pin(letters[1][l], 0);
  }
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void set_pwm_pin(int pin, float val) {
  val = (val * 256) * 16;
  pwm.setPWM(pin, 4096 - val, val);
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void set_color_bar(float red, float green, float blue, float white){
  green = 1.0 - green;
  white = 1.0 - white;
  analogWrite(red_led, red * 255);
  set_pwm_pin(green_pwm_pin, green);
  analogWrite(blue_led, blue * 255);
  set_pwm_pin(white_pwm_pin, white);
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void set_peltiers_percentage(float a_state, float b_state){
//  Serial.print("Peltiers set to A: ");Serial.print(a_state);
//  Serial.print(" B: ");Serial.println(b_state);
  PELTIER_A_DUTY_CYCLE = a_state;
  PELTIER_B_DUTY_CYCLE = a_state;
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

const float PELTIER_CYCLE_MS = 100.0;
unsigned long peltier_cycle_timestamp = 0;
unsigned long now = 0;
int peltiers_state[2] = {LOW, LOW};

void update_peltier_cycle() {
  now = millis();
  // did the cycle just begin?
  if (now < peltier_cycle_timestamp + PELTIER_CYCLE_MS) {
    peltier_cycle_timestamp = now;
    // turn one of them on
    if (PELTIER_A_DUTY_CYCLE > 0 && PELTIER_B_DUTY_CYCLE == 0) {
      if (now < peltier_cycle_timestamp + int(PELTIER_A_DUTY_CYCLE * PELTIER_CYCLE_MS)) {
        if (
      }
    }
    else if (PELTIER_B_DUTY_CYCLE > 0 && PELTIER_A_DUTY_CYCLE == 0) {
      //
    }
    else {
      disable_peltiers();
    }
  }
  // is it time to turn them off?
  else if (peltiers_state[0] || peltiers_state[1]){
    
  }
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void write_h_bridges(int a_state, int b_state) {
  digitalWrite(peltier_ab_enable, HIGH);
  analogWrite(peltier_a_control, a_state);
  analogWrite(peltier_b_control, b_state);
  peltiers_state[0] = a_state;
  peltiers_state[1] = b_state;
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void disable_peltiers(){
//  Serial.println("Peltiers are both OFF");
  digitalWrite(peltier_ab_enable, LOW);
  digitalWrite(peltier_a_control, LOW);
  digitalWrite(peltier_b_control, LOW);
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void set_fan_pwm(int pwm){
//  Serial.print("Writing fan: ");Serial.println(pwm);
  analogWrite(fan_pwm, pwm);
}

void set_fan_percentage(float percentage){
  if (percentage < 0) percentage = 0;
  if (percentage > 1) percentage = 1;
  analogWrite(fan_pwm, int(percentage * 255.0));
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

float average_adc(int pin, int numsamples=5) {
  uint8_t i;
  float samples[numsamples];
  float average;
  for (i=0; i< numsamples; i++) {
   samples[i] = analogRead(pin);
   delayMicroseconds(50);
  }
  average = 0;
  for (i=0; i< numsamples; i++) {
     average += samples[i];
  }
  return average /= numsamples;
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

float peltier_temperature(float avg_adc=-1){
  if (avg_adc < 0) {
    avg_adc = average_adc(thermistor, 5);
  }
  if (avg_adc < TABLE[TABLE_SIZE-1][0]) {
    return -1;
  }
  else if (avg_adc > TABLE[0][0]){
    return 100;
  }
  else {
    int ADC_LOW, ADC_HIGH;
    for (int i=0;i<TABLE_SIZE - 1;i++){
      ADC_HIGH = TABLE[i][0];
      ADC_LOW = TABLE[i+1][0];
      if (avg_adc >= ADC_LOW && avg_adc <= ADC_HIGH){
        float adc_total_diff = abs(ADC_HIGH - ADC_LOW);
        float percent_from_colder = float(abs(ADC_HIGH - avg_adc)) / adc_total_diff;
        float temp_diff = float(abs(TABLE[i+1][1] - TABLE[i][1]));
        float thermister_temp = float(TABLE[i][1]) + (percent_from_colder * temp_diff);

        // then convert
        return (thermister_temp * 0.937685) + 2.113056;
      }
    }
  }
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void update_target_temperature(){
  if (IS_TARGETING) {
    float temp = peltier_temperature();
    // if we've arrived, just be calm, but don't turn off
    if (int(temp) == TARGET_TEMPERATURE) {
      set_color_bar(0, 1, 0, 0);
      if (TARGET_TEMPERATURE > 25.0) set_hot_percentage(0.5);
      else set_cold_percentage(1.0);
    }
    else if (TARGET_TEMPERATURE - int(temp) > 0.0) {
      if (TARGET_TEMPERATURE > 25.0) set_hot_percentage(1.0);
      else set_hot_percentage(0.2);
    }
    else {
      if (TARGET_TEMPERATURE < 25.0) set_cold_percentage(1.0);
      else set_cold_percentage(0.2);
    }
  }
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void update_temperature_display(boolean force=false){
  int current_temp = peltier_temperature();
  boolean stable = true;
  for (int i=0;i<100;i++){
    if (int(peltier_temperature()) != current_temp) {
      stable = false;
      break;
    }
  }
  if (stable || force) {
    if (current_temp != prev_temp || force){
      display_number(current_temp);
    }
    prev_temp = current_temp;
  }
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

boolean DOING_CYCLE_TEST = false;

const int cycle_high = 90;
const int cycle_low = 5;

void cycle_test(){
  set_fan_percentage(1.0);
  if (TARGET_TEMPERATURE == int(peltier_temperature())){
    if (TARGET_TEMPERATURE == cycle_high){
      set_target_temperature(cycle_low);
    }
    else {
      set_target_temperature(cycle_high);
    }
  }
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void read_serial(){
  if (Serial.available()){
    char letter = Serial.read();
    int val;
    if (letter == 'c'){
      val = Serial.parseInt();
      set_cold_percentage(float(val) / 100.0);
      disable_target();
    }
    else if (letter == 'h'){
      val = Serial.parseInt();
      set_hot_percentage(float(val) / 100.0);
      disable_target();
    }
    else if (letter == 'x'){
      DOING_CYCLE_TEST = false;
      disable_peltiers();
      set_color_bar(0, 0, 0, 1);
      disable_target();
      set_fan_percentage(0.0);
    }
    else if (letter == 'f'){
      val = Serial.parseInt();
      set_fan_percentage(float(val) / 100.0);
    }
    else if (letter == 't'){
      val = Serial.parseInt();
      set_target_temperature(val);
    }
    else if (letter == 'p') {
      Serial.print(average_adc(thermistor, 5));
      Serial.print("\t");
      Serial.print(peltier_temperature());
      Serial.println(" deg-C");
    }
    else if (letter == 'l'){
      startup_color_animation();
    }
    else if (letter == 'r') {
      DOING_CYCLE_TEST = true;
      set_target_temperature(cycle_low);
    }
    while (Serial.available() > 0) {
      Serial.read();
    }
    Serial.println("ok");
    Serial.println("ok");
  }
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void setup() {
  setup_pins();
  disable_peltiers();
  set_fan_pwm(0);
  setup_lights();
  startup_color_animation();
  set_color_bar(0, 0, 0, 1);
  update_temperature_display(true);

  Serial.begin(115200);
  Serial.setTimeout(30);
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

void loop(){
  read_serial();
  update_temperature_display();
  update_target_temperature();
  if (DOING_CYCLE_TEST){
    cycle_test();
  }
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////
