#include "errors.hpp"

#include <Arduino.h>
#include <ArduinoEigen/Eigen/Geometry>
#include <ArduinoLog.h>
#include <BleMouse.h>
#include <Wire.h>

#include <cmath>
#include <limits>

#define ICM_20948_USE_DMP
#include <ICM_20948.h>

/**
 * @brief converts from a wider data type to a narrower one, capping the output value to prevent undefined behavior
 *
 * @param n the value to be converted
 */
template <typename Smaller, typename Larger> [[nodiscard]] Smaller saturate_cast(Larger n) noexcept {
  if (n > std::numeric_limits<Smaller>::max()) {
    return std::numeric_limits<Smaller>::max();
  } else if (n < std::numeric_limits<Smaller>::min()) {
    return std::numeric_limits<Smaller>::min();
  } else {
    return static_cast<Smaller>(n);
  }
}

/**
 * @brief Transforms a given vector based on the orientation recorded by the DMP.
 *
 * @details Given a vector in Arduino-space (i.e., basis vectors relative to the ICM), uses the ICM
 * sensor data to transform it into a vector in world-space (i.e., z parallel to gravity, y parallel
 * to magnetic north, x parallel to magnetic east).
 *
 * @param vec the vector in Arduino-space to be transformed
 * @param packet the sensor data packet from the DMP
 * @pre the packet must contain valid orientation-mode 9-DOF Quat9 data.
 */
[[nodiscard]] Eigen::Vector3d toWorldSpace(const Eigen::Vector3d vec, const icm_20948_DMP_data_t packet) noexcept {
  const double scalingFactor = std::pow(2.0, 30.0);
  const double w = ((double)packet.Quat9.Data.Q1) / scalingFactor;
  const double x = ((double)packet.Quat9.Data.Q2) / scalingFactor;
  const double y = ((double)packet.Quat9.Data.Q3) / scalingFactor;
  const double z = std::sqrt(1.0 - (x * x + y * y + z * z));
  const Eigen::Quaternion<double> quat(w, x, y, z);
  return quat._transformVector(vec);
}

/**
 * @class Screen
 * @brief Represents the viewscreen the mouse will be on
 */
class Screen {
private:
  Eigen::Vector3d m_center;
  Eigen::Vector3d m_forward;
  Eigen::Vector3d m_right;
  Eigen::Vector3d m_up;

public:
  /**
   * @param sensitivity the factor by which changes in the mouse angle are multiplied
   * @param forward a vector normal to the plane of the screen and pointing out the back
   * @param up a vector parallel to the plane of the screen and pointing straight up it
   */
  Screen(double sensitivity, Eigen::Vector3d forward, Eigen::Vector3d up) noexcept
      : m_center(forward.normalized() * sensitivity), m_forward(forward.normalized()), m_up(up.normalized()),
        m_right(m_forward.cross(m_up)) {}

  /**
   * @brief recalibrate the position of the screen in space
   *
   * @param sensitivity factor by which changed in the mouse angle are multiplied
   * @param forward a vector normal to the plane of the screen and pointing out the back
   * @param up a vector parallel to the plane of the screen and pointing straight up it
   */
  void recalibrate(double sensitivity, Eigen::Vector3d forward, Eigen::Vector3d up) noexcept {
    m_center = forward.normalized() * sensitivity;
    m_forward = forward.normalized();
    m_up = up.normalized();
    m_right = m_forward.cross(m_up);
  }

  /**
   * @brief determines a pixel position the 'mouse' is pointing at
   *
   * @details The screen is treated as being `sensitivity` pixels away from the mouse. If the mouse is facing away
   * from the screen, an imaginary vector is drawn backwards from the mouse and calculations are performed using
   * this. If the mouse is exactly parallel with the screen, behavior is undefined. Pixel positions are given
   * relative to the center of the screen and should not be treated as absolute.
   * @param mouseDirection a unit vector denoting the 'forward' vector of the mouse
   */
  [[nodiscard]] Eigen::Vector2<long> project(const Eigen::Vector3d mouseDirection) const noexcept {
    const Eigen::Hyperplane<double, 3> screenPlane(m_forward, m_center);
    const Eigen::ParametrizedLine<double, 3> ray(Eigen::Vector3d::Zero(), mouseDirection);
    const Eigen::Vector3d intersectionPoint = ray.intersectionPoint(screenPlane);
    return Eigen::Vector2<long>((long)m_right.dot(intersectionPoint - m_center),
                                (long)m_up.dot(intersectionPoint - m_center));
  }
};

ICM_20948_I2C icm;
BleMouse mouse("Mouseless Mouse", "Mouseless Mouse Team"); // Initialize Bluetooth mouse object to send mouse events
Screen screen(100, Eigen::Vector3d(0, 1, 0), Eigen::Vector3d(0, 0, 1));

volatile bool buttonPress = false;
void IRAM_ATTR onButtonPress() { buttonPress = true; }

Eigen::Vector2<long> mousePosition;

void setup() {
  constexpr uint8_t BUTTON_PIN = 14;
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(BUTTON_PIN, onButtonPress, FALLING);

  // starts Serial Monitor
  Serial.begin(115200);
  Log.begin(LOG_LEVEL_VERBOSE, &Serial);

  if (!icm.begin() || icm.initializeDMP() != ICM_20948_Stat_Ok ||
      icm.enableDMPSensor(INV_ICM20948_SENSOR_ORIENTATION) != ICM_20948_Stat_Ok ||
      icm.setDMPODRrate(DMP_ODR_Reg_Quat9, 0) != ICM_20948_Stat_Ok || icm.enableFIFO() != ICM_20948_Stat_Ok ||
      icm.enableDMP() != ICM_20948_Stat_Ok || icm.resetFIFO() != ICM_20948_Stat_Ok ||
      icm.resetDMP() != ICM_20948_Stat_Ok) {
    std::string error_message = "ICM initialization failed with error status \"";
    mlm::errors::doFatalError((error_message + icm.statusString() + "\"").c_str(),
                              mlm::errors::HARDWARE_INITIALIZATION_FAILED);
  } else {
    Log.infoln("ICM successfully initialized");
  }

  icm_20948_DMP_data_t dataFrame;
  for (;;) {
    icm.readDMPdataFromFIFO(&dataFrame);
    if (icm.status != ICM_20948_Stat_Ok && icm.status != ICM_20948_Stat_FIFOMoreDataAvail) {
      Log.traceln("Waiting for first DMP packet");
      delay(20);
    } else {
      Log.traceln("First DMP packet received");
      mousePosition = screen.project(toWorldSpace(Eigen::Vector3d(0, 1, 0), dataFrame));
      break;
    }
  }

  mouse.begin();
  Log.infoln("Bluetooth mouse successfully initialized");
}

void loop() {
  if (buttonPress) {
    if (mouse.isConnected()) {
      mouse.click();
    } else {
      Log.noticeln("Click registered, but mouse not connected");
    }
    buttonPress = false;
  }

  icm_20948_DMP_data_t dataFrame;
  icm.readDMPdataFromFIFO(&dataFrame);
  if (icm.status == ICM_20948_Stat_Ok || icm.status == ICM_20948_Stat_FIFOMoreDataAvail) {
    Eigen::Vector2<long> diff = screen.project(toWorldSpace(Eigen::Vector3d(0, 1, 0), dataFrame)) - mousePosition;
    mouse.move(saturate_cast<signed char>(diff.x()), saturate_cast<signed char>(diff.y()));
  }
  if (icm.status != ICM_20948_Stat_FIFOMoreDataAvail) {
    delay(10);
  }
}