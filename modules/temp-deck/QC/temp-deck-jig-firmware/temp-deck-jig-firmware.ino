#include <Wire.h>
#include <Adafruit_MLX90614.h>

Adafruit_MLX90614 mlx = Adafruit_MLX90614();

bool auto_print = false;
unsigned long print_timestamp = 0;
const int print_interval = 250;

void setup() {
  Serial.begin(9600);
  mlx.begin();
  Serial.println("Starting!!");
}

void loop() {
    if (Serial.available()) {
        if (Serial.peek() == '1') {
            auto_print = true;
        }
        else if (Serial.peek() == '0') {
            auto_print = false;
        }
        else if (!auto_print) {
            Serial.println(mlx.readObjectTempC());
        }
        while (Serial.available()) {
            Serial.read();
            delay(2);
        }
    }
    if (auto_print && print_timestamp + print_interval < millis()) {
        print_timestamp = millis();
        Serial.println(mlx.readObjectTempC());
    } 
}
