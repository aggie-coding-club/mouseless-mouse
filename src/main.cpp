#include <Arduino.h>
#include <Adafruit_MPU6050.h>
#include <Wire.h>
#include <BleMouse.h>

#define BUTTON_PIN 14 // Which pin is the mouse click button connected to?
#define AVG_SIZE 20   // How many inputs will we keep in rolling average array?

Adafruit_MPU6050 mpu;                                // Initialize MPU - tracks rotation in terms of acceleratioin + gyroscope
BleMouse mouse("Mouseless Mouse", "Espressif", 100); // Initialize bluetooth mouse to send mouse events

inline int sign(float inVal)
{
  /*Returns 1 if input is greater than 0, 0 if input is 0, or -1 if input is less than 0*/
  return (inVal > 0) - (inVal < 0);
}

class RollingAverage
{
  /*Used to keep track of past inputed values to try to smooth movement*/
private:
  float avg[AVG_SIZE];
  bool isInit = false; // Is the avg buffer full of samples?
  uint8_t head = 0;    // Index of most recent value
  float avgVal;
  uint8_t dStable = 0; // How stable the direction of the movement is

public:
  RollingAverage() {}
  ~RollingAverage() {}

  void update(float val)
  {
    /*Update Rolling array with input*/
    if (isInit)
    {
      this->avg[++this->head %= AVG_SIZE] = val;
      float sum = 0;
      int dir = 0;
      for (uint8_t i = (this->head + 1) % AVG_SIZE; i != this->head; ++i %= AVG_SIZE)
      {
        sum += this->avg[i];
        dir += sign(this->avg[(i + 1) % AVG_SIZE] - this->avg[i]);
      }
      this->avgVal = sum / AVG_SIZE;
      this->dStable = abs(dir);
    }
    else
    {
      for (uint8_t i = 0; i < AVG_SIZE; i++)
        this->avg[++this->head %= AVG_SIZE] = val;
      this->avgVal = val;
      isInit = true;
    }
  }

  uint8_t stability()
  {
    /*Returns how consistant is the recent input with eachother?*/
    return this->dStable;
  }

  float get()
  {
    /*Returns the average value of recent inputs*/
    return this->avgVal;
  }
};

volatile bool buttonPress = false; // Is the button currently pressed?

// Initialize values to be smoothed later on
RollingAverage roll;
RollingAverage pitch;
RollingAverage gRoll;
RollingAverage gPitch;

// set previous inputs to 0
float oldRoll = 0;
float oldPitch = 0;

// sets up a function that can interupt and immediately be ran
void IRAM_ATTR onButtonPress()
{
  buttonPress = true;
}

[[noreturn]] void errNoMPU()
{
  Serial.println("Could not find MPU");
  pinMode(LED_BUILTIN, OUTPUT);

  // Endlessly loop on LED flash pattern of short flash, long flash
  for (;;)
  {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(500);
    digitalWrite(LED_BUILTIN, LOW);
    delay(1000);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(1000);
    digitalWrite(LED_BUILTIN, LOW);
    delay(1000);
  }
}

void setup()
{
  // put your setup code here, to run once:
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(BUTTON_PIN, onButtonPress, FALLING); // when Button_pin goes from High to low (button is released) onButtonPress is ran

  // starts Serial Monitor
  Serial.begin(115200);

  if (!mpu.begin())
  {
    errNoMPU();
  }
  else
  {
    Serial.println("Found MPU6050");
  }

  Serial.println("Starting Bluetooth mouse");
  mouse.begin();
}

void loop()
{
  // put your main code here, to run repeatedly:

  sensors_event_t a, g, temp; // sets events (special class)
  mpu.getEvent(&a, &g, &temp);

  // button press stuff
  if (buttonPress)
  {
    if (mouse.isConnected())
    {
      mouse.click();
    }
    else
      Serial.println("Mouse not connected!");
    buttonPress = false;
  }

  roll.update(-atan2(a.acceleration.x, a.acceleration.z)); // weird math to calculate direction change for roll (currently x-axis) then add it to array of prior inputs
  pitch.update(-atan2(a.acceleration.y, a.acceleration.z));
  gRoll.update(g.gyro.y);
  gPitch.update(g.gyro.x); // adding gyroscope in x axis

  // currently using information from gyroscope for if statements
  if (gPitch.stability() > 8)
  { // if the stability is decent

    if (mouse.isConnected())
    {                                                // If the mouse is connected
      mouse.move(0, (pitch.get() - oldPitch) * 100); // move mouse by current pitch (of accelerometer) - old pitch (of accelerometer)
    }
    oldPitch = pitch.get();                                             // update old pitch
    Serial.printf("Pitch: %f (%d)\n", pitch.get(), gPitch.stability()); // prints accelerometer stability and (gyroscope stability)
  }
  if (gRoll.stability() > 8)
  { // currently using information from gyro

    if (mouse.isConnected())
    {
      mouse.move((roll.get() - oldRoll) * 100, 0);
    }
    oldRoll = roll.get();
    Serial.printf("Roll: %f (%d)\n", roll.get(), gRoll.stability());
  }

  delay(10);
}