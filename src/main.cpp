#include <Arduino.h>
#include <Adafruit_MPU6050.h>
#include <Wire.h>

#define BUTTON_PIN 14

Adafruit_MPU6050 mpu;

volatile bool printMotion = false;

void IRAM_ATTR getMPU() {
  printMotion = true;
}

void setup() {
  // put your setup code here, to run once:
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(BUTTON_PIN, getMPU, FALLING);
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
  if (printMotion) {
    Serial.println("Button pressed!");
    printMotion = false;
  }
  Serial.printf("Rotation: %f, %f, %f\n", g.gyro.x, g.gyro.y, g.gyro.z);
  delay(50);
}