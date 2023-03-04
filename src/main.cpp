#include "errors.hpp"

#include <Arduino.h>
#include <Adafruit_MPU6050.h>
#include <Wire.h>
#include <BleMouse.h>
#include <ArduinoLog.h>
#include <array>
#include <type_traits>

#define BUTTON_PIN 14          // Which pin is the mouse click button connected to?
#define ROLLING_BUFFER_SIZE 20 // How many inputs will we keep in rolling average array?

Adafruit_MPU6050 mpu;                                // Initialize MPU - tracks rotation in terms of acceleration + gyroscope
BleMouse mouse("Mouseless Mouse", "Espressif", 100); // Initialize bluetooth mouse to send mouse events

[[nodiscard]] long sign(float inVal)
{
  return (inVal > 0) ? 1 : -1;
}

/**
 * Tracks the rolling average of a series of sampled floats.
 *
 * All samples are given equal weight. A maximum of buffer_length samples will be retained.
 */
template <std::size_t buffer_length>
class RollingAverage
{
private:
  std::array<float, buffer_length> m_buffer{0.0f};
  std::size_t m_next = 0;
  std::size_t m_num_values = 0;
  long m_direction_stability = 0;
  float m_average = 0.0f;

public:
  // Guarantee stability() will never overflow
  static_assert(buffer_length < std::numeric_limits<long>::max(),
                "buffer_length must be shorter than the maximum magnitude of a long");

  /**
   * Adds another value to the rolling average buffer.
   *
   * Also updates the running average and directional stability measurements. If the buffer is full, val will
   * replace the oldest value in the buffer.
   *
   * @param val the value to be added
   * @pre The magnitude of val must be less than the quotient of the greatest finite float value divided by
   *  buffer_length.
   */
  void update(float val)
  {
    if (m_num_values > 0)
    {
      float preceding = (m_next == 0) ? m_buffer[buffer_length - 1] : m_buffer[m_next - 1];
      if (m_num_values == buffer_length)
      {
        // Only roll back stability if we're replacing a value, not just adding one
        m_direction_stability -= sign(m_buffer[m_next] - preceding);
      }
      m_direction_stability += sign(val - preceding);
    }

    float overall_sum = m_average * m_num_values;
    overall_sum -= m_buffer[m_next]; // Depends on buffer being 0.0f-initialized
    overall_sum += val;

    if (m_num_values < buffer_length)
    {
      m_num_values++;
    }
    m_average = overall_sum / m_num_values;
    m_buffer[m_next] = val;

    m_next++;
    m_next %= buffer_length;
  }

  /**
   * Measures the "stability" of the samples over time.
   *
   * The stability is measured as an unsigned long integer whose magnitude depends on the length of the buffer. If the
   * samples are uniformly increasing or decreasing, then the stability will be equal to the length of the buffer.
   * Alternatively, if the samples are alternating up and down, stability will be measured as zero.
   */
  [[nodiscard]] long stability() const
  {
    return std::abs(m_direction_stability);
  }

  /**
   * Gets the mean of the last buffer_length samples.
   *
   * Returns zero if no samples have been added. If less than buffer_length samples have been added, then returns
   * the average of all samples taken.
   */
  [[nodiscard]] float get() const noexcept
  {
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

void setup()
{
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(BUTTON_PIN, onButtonPress, FALLING); // when Button_pin goes from High to low (button is released) onButtonPress is ran

  // starts Serial Monitor
  Serial.begin(115200);
  Log.begin(LOG_LEVEL_VERBOSE, &Serial);

  if (!mpu.begin())
  {
    mlm::errors::doFatalError("no MPU detected", mlm::errors::HARDWARE_INITIALIZATION_FAILED);
  }
  else
  {
    Log.infoln("Found MPU6050");
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    Log.infoln("Accelerometer range set to +-8 G");
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    Log.infoln("Gyrometer range set to +-500 deg");
    mpu.setFilterBandwidth(MPU6050_BAND_44_HZ);
    Log.infoln("Low-pass filter frequency set to 44 Hz");
    mpu.setHighPassFilter(MPU6050_HIGHPASS_0_63_HZ); // This choice of value might be problematic, needs testing lol
    Log.infoln("High-pass filter frequency set to 0.63 Hz");
  }

  mouse.begin();
  Log.infoln("Started Bluetooth mouse");
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
      Log.noticeln("Mouse not connected!");
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
    oldPitch = pitch.get();                                                          // update old pitch
    Log.trace("Pitch: %f rad (stability of %d)\n", pitch.get(), gPitch.stability()); // prints accelerometer stability and (gyroscope stability)
  }
  if (gRoll.stability() > 8)
  { // currently using information from gyro

    if (mouse.isConnected())
    {
      mouse.move((roll.get() - oldRoll) * 100, 0);
    }
    oldRoll = roll.get();
    Log.trace("Roll: %f rad (stability of %d)\n", roll.get(), gRoll.stability());
  }

  delay(10);
}