  #include <Wire.h>
#include <Adafruit_MLX90614.h>

Adafruit_MLX90614 mlx = Adafruit_MLX90614();

void setup() {
  Serial.begin(115200);
  mlx.begin();  
}

void loop() {
   if (Serial.available()) {
    Serial.println(mlx.readObjectTempC());
     while (Serial.available()) {
       Serial.read();
     }
   }
}
