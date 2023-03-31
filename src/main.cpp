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
BleMouse mouse("Mouseless Mouse", "Mouseless Team"); // Initialize Bluetooth mouse object to send mouse events

/// @brief Convert a vector from mouse-space to world-space.
/// @details In mouse-space, +x, +y, and +z are defined in accordance with the accelerometer axes defined in the ICM
/// 20948 datasheet. In world-space, +x is due east, +y is due north, and +z is straight up.
/// @param vec The vector in mouse-space.
/// @param icm A reference to the ICM_20948_I2C object whose orientation data is being used.
/// @return The equivalent of `vec` in world-space.
[[nodiscard]] Eigen::Vector3f mouseSpaceToWorldSpace(Eigen::Vector3f vec, ICM_20948_I2C &icm) noexcept {
  Eigen::Vector3f up = {icm.accX(), icm.accY(), icm.accZ()};
  up.normalize();
  Eigen::Vector3f north = {icm.magX(), -icm.magY(), -icm.magZ()};
  north.normalize();
  north -= up * up.dot(north);
  north.normalize();
  Eigen::Vector3f east = north.cross(up);
  return Eigen::Vector3f{
      east.dot(vec),
      north.dot(vec),
      up.dot(vec),
  };
}

void setup() {
  Serial.begin(115200); // Start the serial console
  Log.begin(LOG_LEVEL_VERBOSE, &Serial);
  delay(100);
  Wire.begin();
  Wire.setClock(400000);

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
    icm.getAGMT();
    calibratedPosX = mouseSpaceToWorldSpace(Eigen::Vector3f{1.0f, 0.0f, 0.0f}, icm);
    calibratedPosZ = mouseSpaceToWorldSpace(Eigen::Vector3f{0.0f, 0.0f, 1.0f}, icm);
    while (Serial.available() > 0) {
      Serial.read();
    }
    Serial.print("Now enter a sensitivity value (integer, 1-127): ");
    long accumulator = Serial.parseInt();
    while (accumulator < 1 || accumulator > 127) {
      Serial.print("Invalid or no input. Please enter an integer from 1-127 inclusive: ");
      accumulator = Serial.parseInt();
    }
    sensitivity = static_cast<signed char>(accumulator);
    hasCalibrated = true;
  }
  if (icm.dataReady()) {
    icm.getAGMT();
    Eigen::Vector3f posY = mouseSpaceToWorldSpace(Eigen::Vector3f{0.0f, 1.0f, 0.0f}, icm);
    signed char xMovement = posY.dot(calibratedPosX) * sensitivity;
    signed char zMovement = posY.dot(calibratedPosZ) * sensitivity;
    mouse.move(xMovement, zMovement);
    delay(30);
  } else {
    Log.noticeln("Waiting for data");
    delay(500);
  }
}