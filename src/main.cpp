#include <Arduino.h>
#include <Adafruit_MPU6050.h>
#include <Wire.h>
#include <BleMouse.h>

#define BUTTON_PIN 14 //Which pin is the mouse click button connected to?
#define AVG_SIZE 20 //How many inputs will we keep in rolling average array?

Adafruit_MPU6050 mpu; //Initialize MPU - tracks rotation in terms of acceleratioin + gyroscope
BleMouse mouse("Mouseless Mouse", "Espressif", 100); //Initialize bluetooth mouse to send mouse events


inline int sign(float inVal) {
  /*Returns 1 if input is greater than 0, 0 if input is 0, or -1 if input is less than 0*/
  return (inVal > 0) - (inVal < 0);
}

class RollingAverage {
/*Used to keep track of past inputed values to try to smooth movement*/
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
    /*Update Rolling array with input*/
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
    /*Returns how consistant is the recent input with eachother?*/
    return this->dStable;
  }

  float get() {
    /*Returns the average value of recent inputs*/
    return this->avgVal;
  }
};

volatile bool buttonPress = false;//Is the button currently pressed?

//Initialize values to be smoothed later on
RollingAverage roll;
RollingAverage pitch;
RollingAverage gRoll;
RollingAverage gPitch;


//set previous inputs to 0
float oldRoll = 0;
float oldPitch = 0;

//sets up a function that can interupt and immediately be ran
void IRAM_ATTR onButtonPress() {
  buttonPress = true;
}

void setup() {
  // put your setup code here, to run once:
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(BUTTON_PIN, onButtonPress, FALLING);//when Button_pin goes from High to low (button is released) onButtonPress is ran

  //starts Serial Monitor
  Serial.begin(115200);


  if (!mpu.begin()) {//if MPU can't connect
    Serial.println("Could not find MPU");
    while(1);
  }

  Serial.println("Found MPU6050");
  Serial.println("Starting Bluetooth mouse");
  mouse.begin();//starts Bluetooth mouse
}

void loop() {
  // put your main code here, to run repeatedly:
  digitalWrite(12,HIGH);
  delay(900);
  digitalWrite(12,LOW);
  delay(650);
}