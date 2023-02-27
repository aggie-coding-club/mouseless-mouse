#include <Arduino.h>
#include <Adafruit_MPU6050.h>
#include <Wire.h>
#include <BleMouse.h>

#include <array>

#define BUTTON_PIN 14          // Which pin is the mouse click button connected to?
#define ROLLING_BUFFER_SIZE 20 // How many inputs will we keep in rolling average array?

Adafruit_MPU6050 mpu;                                // Initialize MPU - tracks rotation in terms of acceleratioin + gyroscope
BleMouse mouse("Mouseless Mouse", "Espressif", 100); // Initialize bluetooth mouse to send mouse events

[[nodiscard]] long sign(float inVal)
{
  return (inVal > 0) ? 1 : -1;
}

template <std::size_t buffer_length>
class RollingAverage
{
  /*Used to keep track of past inputed values to try to smooth movement*/
private:
  std::array<float, buffer_length> m_buffer{0.0f}; // Used to implement a ring buffer
  std::size_t m_to_drop = 0;                       // Index of most recent value
  std::size_t m_num_values = 0;
  long m_direction_stability = 0; // How stable the direction of the movement is
  float m_average = 0.0f;

public:
  void update(float val)
  {
    if (m_num_values > 0)
    {
      float preceding = (m_to_drop == 0) ? m_buffer[buffer_length - 1] : m_buffer[m_to_drop - 1];
      m_direction_stability -= sign(m_buffer[m_to_drop] - preceding);
      m_direction_stability += sign(val - preceding);
    }

    float overall_sum = m_average * m_num_values;
    overall_sum -= m_buffer[m_to_drop];
    overall_sum += val;

    if (m_num_values < buffer_length)
    {
      m_num_values++;
    }
    m_average = overall_sum / m_num_values;
    m_buffer[m_to_drop] = val;

    m_to_drop++;
    m_to_drop %= buffer_length;
  }

  [[nodiscard]] long stability() const
  {
    /*Returns how consistant is the recent input with eachother?*/
    return std::abs(m_direction_stability);
  }

  [[nodiscard]] float get() const noexcept
  {
    /*Returns the average value of recent inputs*/
    return m_average;
  }
};

volatile bool buttonPress = false; // Is the button currently pressed?

// Initialize values to be smoothed later on
RollingAverage<ROLLING_BUFFER_SIZE> roll;
RollingAverage<ROLLING_BUFFER_SIZE> pitch;
RollingAverage<ROLLING_BUFFER_SIZE> gRoll;
RollingAverage<ROLLING_BUFFER_SIZE> gPitch;

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
    {
      Serial.println("Mouse not connected!");
    }
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