#include <Arduino.h>

void setup() {
  // put your setup code here, to run once:
  pinMode(12,OUTPUT);
}

void loop() {
  // put your main code here, to run repeatedly:
  digitalWrite(12,HIGH);
  delay(1000);
  digitalWrite(12,LOW);
<<<<<<< HEAD
  delay(650);
=======
  delay(1000);
>>>>>>> 6baa8514517e97263d17e4bee37e470937c7683c
}