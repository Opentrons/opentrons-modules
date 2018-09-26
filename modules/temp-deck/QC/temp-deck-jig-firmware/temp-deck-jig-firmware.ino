#include "thermistor.h"

Thermistor thermistor = Thermistor();
float current_temp_thermistor = 0.0;

bool auto_print = false;
unsigned long print_timestamp = 0;
const int print_interval = 250;

void read_thermistor() {
    if (thermistor.update()) {
        current_temp_thermistor = thermistor.temperature();
    }
}

void print_temperature(bool force=false) {
    if (auto_print && print_timestamp + print_interval < millis()) {
        print_timestamp = millis();
        Serial.println(current_temp_thermistor);
    }
    else if (!auto_print && force) {
        Serial.println(current_temp_thermistor);
    }
}

void setup() {
    Serial.begin(115200);
    while (!thermistor.update()) {}
    current_temp_thermistor = thermistor.temperature();
}

void loop() {
    read_thermistor();
    print_temperature();
    if (Serial.available()) {
        if (Serial.peek() == '1') {
            auto_print = true;
        }
        else if (Serial.peek() == '0') {
            auto_print = false;
        }
        print_temperature(true);
        while (Serial.available()) {
            Serial.read();
            delay(2);
        }
    }
}
