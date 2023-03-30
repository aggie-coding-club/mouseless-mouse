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
/// @details In mouse-space, +x, +y, and +z are defined in accordance with the ICM 20948 datasheet. In world-space, +x
/// is due east, +y is due north, and +z is straight up.
/// @param vec The vector in mouse-space.
/// @param icm A reference to the ICM_20948_I2C object whose orientation data is being used.
/// @return The equivalent of `vec` in world-space.
Eigen::Vector3f mouseSpaceToWorldSpace(Eigen::Vector3f vec, ICM_20948_I2C &icm) {
  Eigen::Vector3f up = {icm.accX(), icm.accY(), icm.accZ()};
  up.normalize();
  Eigen::Vector3f north = {icm.magX(), icm.magY(), icm.magZ()};
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

/// @brief Represents the viewscreen that the mouse is being moved around on.
class Screen {
private:
  Eigen::Vector3f mHorizontal;
  Eigen::Vector3f mVertical;

public:
  /// @brief  Constructs the Screen object based on initial calibration data.
  /// @param upperLeft A normalized vector going from the mouse's position to the upper left corner of the screen.
  /// @param lowerLeft A normalized vector going from the mouse's position to the lower left corner of the screen.
  /// @param lowerRight A normalized vector going from the mouse's position to the lower right corner of the screen.
  Screen(Eigen::Vector3f upperLeft, Eigen::Vector3f lowerLeft, Eigen::Vector3f lowerRight) noexcept
      : mHorizontal((lowerRight - lowerLeft).normalized()), mVertical((upperLeft - lowerLeft).normalized()) {}

  /// @brief Adjusts the Screen object based on new calibration data.
  /// @param upperLeft A normalized vector going from the mouse's position to the upper left corner of the screen.
  /// @param lowerLeft A normalized vector going from the mouse's position to the lower left corner of the screen.
  /// @param lowerRight A normalized vector going from the mouse's position to the lower right corner of the screen.
  void recalibrate(Eigen::Vector3f upperLeft, Eigen::Vector3f lowerLeft, Eigen::Vector3f lowerRight) noexcept {
    mHorizontal = (lowerRight - lowerLeft).normalized();
    mVertical = (upperLeft - lowerLeft).normalized();
  }

  /// @brief Finds the projection of a vector onto the screen.
  /// @param vec A normalized vector with its origin at the mouse.
  /// @return A 2-dimensional vector. The x-axis ranges from 0 at the left of the screen to 1 at the right; similarly,
  /// the y-axis ranges from 0 at the bottom of the screen to 1 at the top.
  [[nodiscard]] Eigen::Vector2f project(Eigen::Vector3f vec) const noexcept {
    return Eigen::Vector2f{vec.dot(mHorizontal) + 0.5f, vec.dot(mVertical) + 0.5f};
  }
};

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

void loop() {
  if (icm.dataReady()) {
    icm.getAGMT();
    delay(30);
  } else {
    Log.noticeln("Waiting for data");
    delay(500);
  }
}