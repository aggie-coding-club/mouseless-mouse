#include <Arduino.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>

Adafruit_MPU6050 mpu;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  delay(100);
  if (!mpu.begin()) {
    Serial.println("Could not find MPU");
    while(1);
  }
  Serial.println("Found MPU6050");
}

void loop() {
  // put your main code here, to run repeatedly:
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  Serial.printf("Rotation: %f, %f, %f\n", g.gyro.x, g.gyro.y, g.gyro.z);
  delay(500);
}