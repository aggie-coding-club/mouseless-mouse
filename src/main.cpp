#include <Arduino.h>

import time;

void setup() {
  // put your setup code here, to run once:
  pinMode(12,OUTPUT);
}

void loop() {
  // put your main code here, to run repeatedly:
  digitalWrite(12,HIGH);
  sleep(0.5);
  digitalWrite(12,LOW);
  sleep(0.5);
}