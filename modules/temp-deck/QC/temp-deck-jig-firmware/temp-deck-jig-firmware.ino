#include <Wire.h>
#include "Adafruit_MLX90614.h"

Adafruit_MLX90614 mlx = Adafruit_MLX90614();
float current_temp_ir = 0.0;
const int ir_sample_size = 10;
int ir_current_sample = 0;
float accumulated_ir_readings = 0;

#include "thermistor.h"

Thermistor thermistor = Thermistor();
float current_temp_thermistor = 0.0;

bool auto_print = false;
unsigned long print_timestamp = 0;
const int print_interval = 250;

void read_ir_sensor() {
    accumulated_ir_readings += mlx.readObjectTempC();
    ir_current_sample += 1;
    if (ir_current_sample == ir_sample_size) {
        current_temp_ir = accumulated_ir_readings / float(ir_sample_size);
        ir_current_sample = 0;
        accumulated_ir_readings = 0.0;
    }
}

void read_thermistor() {
    if (thermistor.update()) {
        current_temp_thermistor = thermistor.temperature();
    }
}

void print_temperatures(bool force=false) {
    if (auto_print && print_timestamp + print_interval < millis()) {
        print_timestamp = millis();
        Serial.print(current_temp_ir);
        Serial.print(',');
        Serial.println(current_temp_thermistor);
    }
    else if (!auto_print && force) {
        Serial.print(current_temp_ir);
        Serial.print(',');
        Serial.println(current_temp_thermistor);
    }
}

void setup() {
    Serial.begin(9600);
    while (!thermistor.update()) {}
    current_temp_thermistor = thermistor.temperature();
    mlx.begin();
    current_temp_ir = mlx.readObjectTempC();
}

void loop() {
    read_thermistor();
    read_ir_sensor();
    print_temperatures();
    if (Serial.available()) {
        if (Serial.peek() == '1') {
            auto_print = true;
        }
        else if (Serial.peek() == '0') {
            auto_print = false;
        }
        print_temperatures(true);
        while (Serial.available()) {
            Serial.read();
            delay(2);
        }
    }
}
