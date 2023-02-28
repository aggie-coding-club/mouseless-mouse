#include <Arduino.h>
#include <Adafruit_MPU6050.h>
#include<Adafruit_BusIO_Register.h>
#include <Wire.h>
#include <BleMouse.h>

#define BUTTON_PIN 14
#define AVG_SIZE 20

Adafruit_MPU6050 mpu;
BleMouse mouse("Mouseless Mouse", "Espressif", 100);


inline int sign(float inVal) {
  /*Returns 1 if input is greater than 0, 0 if input is 0, or -1 if input is less than 0*/
  return (inVal > 0) - (inVal < 0);
}

class RollingAverage {

private:
  float avg[AVG_SIZE];
  bool isInit = false;  // Is the avg buffer full of samples?
  uint8_t head = 0;     // Index of most recent value
  float avgVal;
  uint8_t dStable = 0;  // How stable the direction of the movement is

public:
  RollingAverage () {}
  ~RollingAverage () {}

  void update(float val) {
    if (isInit) {
      this->avg[++this->head%=AVG_SIZE] = val;
      float sum = 0;
      int dir = 0;
      for (uint8_t i=(this->head+1)%AVG_SIZE; i!=this->head; ++i%=AVG_SIZE) {
        sum += this->avg[i];
        dir += sign(this->avg[(i+1)%AVG_SIZE]-this->avg[i]);
      }
      this->avgVal = sum / AVG_SIZE;
      this->dStable = abs(dir);
    }
    else {
      for (uint8_t i=0; i<AVG_SIZE; i++) this->avg[++this->head%=AVG_SIZE] = val;
      this->avgVal = val;
      isInit = true;
    }
  }

  uint8_t stability() {
    return this->dStable;
  }

  float get() {
    return this->avgVal;
  }
};

volatile bool buttonPress = false;

RollingAverage roll;
RollingAverage pitch;
RollingAverage gRoll;
RollingAverage gPitch;

float oldRoll = 0;
float oldPitch = 0;

void IRAM_ATTR onButtonPress() {
  buttonPress = true;
}

void setup() {
  // put your setup code here, to run once:
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(BUTTON_PIN, onButtonPress, FALLING);
  Serial.begin(115200);
  if (!mpu.begin()) {
    Serial.println("Could not find MPU");
    while(1);
  }
  Serial.println("Found MPU6050");
  Serial.println("Starting Bluetooth mouse");
  mouse.begin();
}

void loop() {
  // put your main code here, to run repeatedly:
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  if (buttonPress) {
    if (mouse.isConnected()) {
      mouse.click();
    }
    else Serial.println("Mouse not connected!");
    buttonPress = false;
  }
  roll.update(-atan2(a.acceleration.x, a.acceleration.z));
  pitch.update(-atan2(a.acceleration.y,a.acceleration.z));
  gRoll.update(g.gyro.y);
  gPitch.update(g.gyro.x);
  if (gPitch.stability() > 8) {
    if (mouse.isConnected()) mouse.move(0, (pitch.get() - oldPitch) * 100);
    oldPitch = pitch.get();
    Serial.printf("Pitch: %f (%d)\n", pitch.get(), gPitch.stability());
  }
  if (gRoll.stability() > 8) {
    if (mouse.isConnected()) mouse.move((roll.get() - oldRoll) * 100, 0);
    oldRoll = roll.get();
    Serial.printf("Roll: %f (%d)\n", roll.get(), gRoll.stability());
  }
  delay(10);
}