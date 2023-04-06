#include <Adafruit_LIS3MDL.h>
#include <ArduinoEigen/Eigen/Dense>
#include <ArduinoEigen/Eigen/Geometry>
#include <ArduinoLog.h>
#include <BleMouse.h>
#include <cstdint>

#include "ICM_20948.h"

// The value of the last bit of the I2C address.
// On the SparkFun 9DoF IMU breakout the default is 1, and when the ADR jumper is closed the value becomes 0
constexpr unsigned long AD0_VAL = 1;

ICM_20948_I2C icm;
Adafruit_LIS3MDL lis3mdl;
TwoWire lis3mdlI2C(1);
BleMouse mouse("Mouseless Mouse", "Mouseless Team"); // Initialize Bluetooth mouse object to send mouse events

template <typename T, std::size_t bufferLength> class RollingAverage {
private:
  std::array<T, bufferLength> mBuffer;
  std::size_t mContentLength = 0;
  std::size_t mCursor = 0;
  T mAverage;

public:
  static_assert(bufferLength != 0);

  void update(T next) noexcept {
    if (mContentLength < bufferLength) {
      mContentLength++;
      mAverage *= static_cast<float>(mContentLength - 1) / static_cast<float>(mContentLength);
    } else {
      mAverage -= mBuffer[mCursor] / static_cast<float>(mContentLength);
    }
    mAverage += next / static_cast<float>(mContentLength);
    mBuffer[mCursor] = next;
    mCursor++;
    if (mCursor >= bufferLength) {
      mCursor = 0;
    }
  }
  [[nodiscard]] T get() const noexcept { return mAverage; }
};

/// @brief Convert a vector from mouse-space to world-space.
/// @details In mouse-space, +x, +y, and +z are defined in accordance with the accelerometer axes defined in the ICM
/// 20948 datasheet. In world-space, +x is due east, +y is due north, and +z is straight up.
/// @param vec The vector in mouse-space.
/// @param icm A reference to the ICM_20948_I2C object whose orientation data is being used.
/// @return The equivalent of `vec` in world-space.
[[nodiscard]] Eigen::Vector3f mouseSpaceToWorldSpace(Eigen::Vector3f vec, ICM_20948_I2C &icm,
                                                     Adafruit_LIS3MDL &lis3mdl) noexcept {
  static RollingAverage<Eigen::Vector3f, 12U> up;
  static RollingAverage<Eigen::Vector3f, 12U> north;
  up.update(Eigen::Vector3f(icm.accX(), icm.accY(), icm.accZ()).normalized());
  float magX, magY, magZ;
  lis3mdl.readMagneticField(magX, magY, magZ);
  north.update(Eigen::Vector3f(magX, magY, magZ).normalized());
  Eigen::Vector3f adjusted_north = north.get() - (up.get() * up.get().dot(north.get()));
  adjusted_north.normalize();
  Eigen::Vector3f east = adjusted_north.cross(up.get());
  return Eigen::Vector3f{
      east.dot(vec),
      adjusted_north.dot(vec),
      up.get().dot(vec),
  };
}

void setup() {
  Serial.begin(115200); // Start the serial console
  Log.begin(LOG_LEVEL_VERBOSE, &Serial);
  delay(100);
  Wire.begin();
  Wire.setClock(400000);
  mouse.begin();
  lis3mdlI2C.begin(15, 13);
  if (!lis3mdl.begin_I2C(28U, &lis3mdlI2C)) {
    Log.errorln("IT DIDN'T FUCKIMG START");
  }
  lis3mdl.setPerformanceMode(LIS3MDL_MEDIUMMODE);
  lis3mdl.setOperationMode(LIS3MDL_CONTINUOUSMODE);
  lis3mdl.setDataRate(LIS3MDL_DATARATE_155_HZ);
  lis3mdl.setRange(LIS3MDL_RANGE_4_GAUSS);
  lis3mdl.setIntThreshold(500);
  lis3mdl.configInterrupt(false, false, true, true, false, true);
  for (;;) {
    icm.begin(Wire, AD0_VAL);
    if (icm.status != ICM_20948_Stat_Ok) {
      Log.noticeln("ICM initialization failed with error status \"%s\", retrying", icm.statusString());
      delay(500);
    } else {
      Log.traceln("ICM initialization successful");
      break;
    }
  }
}

bool hasCalibrated = false;
Eigen::Vector3f calibratedPosX;
Eigen::Vector3f calibratedPosZ;
signed char sensitivity;

void loop() {
  if (!hasCalibrated) {
    while (Serial.available() > 0) {
      Serial.read();
    }
    Serial.println("Place mouse into resting position, then press any key to continue.");
    while (Serial.available() == 0) {
      // do nothing
    }
    Serial.println("Mouse calibrated!");
    icm.getAGMT();
    calibratedPosX = mouseSpaceToWorldSpace(Eigen::Vector3f{1.0f, 0.0f, 0.0f}, icm, lis3mdl);
    calibratedPosZ = mouseSpaceToWorldSpace(Eigen::Vector3f{0.0f, 0.0f, 1.0f}, icm, lis3mdl);
    while (Serial.available() > 0) {
      Serial.read();
    }
    // Serial.print("Now enter a sensitivity value (integer, 1-127): ");
    // long accumulator = Serial.parseInt();
    // while (accumulator < 1 || accumulator > 127) {
    //   Serial.print("Invalid or no input. Please enter an integer from 1-127 inclusive: ");
    //   accumulator = Serial.parseInt();
    // }
    sensitivity = 8;
    hasCalibrated = true;
  }
  if (icm.dataReady()) {
    icm.getAGMT();
    Eigen::Vector3f posY = mouseSpaceToWorldSpace(Eigen::Vector3f{0.0f, 1.0f, 0.0f}, icm, lis3mdl);
    signed char xMovement = posY.dot(calibratedPosX) * sensitivity;
    signed char zMovement = posY.dot(calibratedPosZ) * sensitivity;
    mouse.move(xMovement, zMovement);
    delay(30);
  } else {
    Log.noticeln("Waiting for data");
    delay(500);
  }
}