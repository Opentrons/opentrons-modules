#define PIN_BASE 9
#define PIN_READ A1

#define OFF_DELAY_TIME_MS 100
#define TOTAL_SAMPLES 100

#define SERIES_RESISTOR 15.4
#define ADC_BITS 12

int current_sample_index = 0;
double samples[TOTAL_SAMPLES];

double read_adc(bool write_high=true) {
  digitalWrite(PIN_BASE, HIGH);
  int val = analogRead(PIN_READ);
  digitalWrite(PIN_BASE, LOW);
  return val;
}

double get_tec_resistance(){
  double avg_adc_of_resistor = read_adc();
  double avg_adc_of_peltier = (pow(2, ADC_BITS) - 1) - avg_adc_of_resistor;
  double ohms_per_bit = SERIES_RESISTOR / avg_adc_of_resistor;
  double peltier_ohms = ohms_per_bit * avg_adc_of_peltier;
  return peltier_ohms;
}

double get_running_average(double new_ohms){
  if (current_sample_index < TOTAL_SAMPLES) {
    samples[current_sample_index] = new_ohms;
    current_sample_index++;
  }
  else {
    for (int i=0;i<TOTAL_SAMPLES-1;i++){
      samples[i] = samples[i+1];
    }
    samples[TOTAL_SAMPLES-1] = new_ohms;
  }
  double sum_ohms = 0.0;
  for (int i=0;i<current_sample_index;i++){
    sum_ohms += samples[i];
  }
  return sum_ohms / current_sample_index;
}

void setup() {
  pinMode(PIN_BASE, OUTPUT);
  digitalWrite(PIN_BASE, LOW);
  analogReadResolution(ADC_BITS);
  Serial.begin(115200);
}

void loop() {
  delay(OFF_DELAY_TIME_MS);
  Serial.print("Ohms: ");
  Serial.println(get_running_average(get_tec_resistance()), 6);
}
