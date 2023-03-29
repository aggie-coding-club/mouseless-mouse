#define ICM_20948_USE_DMP

#include <ArduinoEigen/Eigen/Dense>
#include <ArduinoEigen/Eigen/Geometry>

#include "ICM_20948.h" // Click here to get the library: http://librarymanager/All#SparkFun_ICM_20948_IMU

#include <BleMouse.h>

// #define USE_SPI       // Uncomment this to use SPI

#define SERIAL_PORT Serial

#define SPI_PORT SPI // Your desired SPI port.       Used only when "USE_SPI" is defined
#define CS_PIN 2     // Which pin you connect CS to. Used only when "USE_SPI" is defined

#define WIRE_PORT Wire // Your desired Wire port.      Used when "USE_SPI" is not defined
// The value of the last bit of the I2C address.
// On the SparkFun 9DoF IMU breakout the default is 1, and when the ADR jumper is closed the value becomes 0
#define AD0_VAL 1

#ifdef USE_SPI
ICM_20948_SPI myICM; // If using SPI create an ICM_20948_SPI object
#else
ICM_20948_I2C myICM; // Otherwise create an ICM_20948_I2C object
#endif

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
  const double z = std::sqrt(1.0 - (w * w + x * x + y * y));
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
  Eigen::Vector3d m_up;
  Eigen::Vector3d m_right;

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
BleMouse mouse("Mouseless Mouse", "Mouseless Team"); // Initialize Bluetooth mouse object to send mouse events
Screen screen(100, Eigen::Vector3d(0, 1, 0), Eigen::Vector3d(0, 0, 1));
Eigen::Vector2<long> mousePosition = {0, 0};

[[nodiscard]] Eigen::Vector3d getEulerAngles(const icm_20948_DMP_data_t packet) noexcept {
  const double scalingFactor = std::pow(2.0, 30.0);
  const double w = ((double)packet.Quat9.Data.Q1) / scalingFactor;
  const double x = ((double)packet.Quat9.Data.Q2) / scalingFactor;
  const double y = ((double)packet.Quat9.Data.Q3) / scalingFactor;
  const double z = std::sqrt(1.0 - (w * w + x * x + y * y));
  const Eigen::Quaternion<double> quat(w, x, y, z);
  return quat.toRotationMatrix().eulerAngles(0, 1, 2);
}

void setup() {

  SERIAL_PORT.begin(115200); // Start the serial console

  delay(100);

#ifdef USE_SPI
  SPI_PORT.begin();
#else
  WIRE_PORT.begin();
  WIRE_PORT.setClock(400000);
#endif

  bool initialized = false;
  while (!initialized) {
#ifdef USE_SPI
    myICM.begin(CS_PIN, SPI_PORT);
#else
    myICM.begin(WIRE_PORT, AD0_VAL);
#endif

    SERIAL_PORT.print(F("Initialization of the sensor returned: "));
    SERIAL_PORT.println(myICM.statusString());
    if (myICM.status != ICM_20948_Stat_Ok) {
      SERIAL_PORT.println(F("Trying again..."));
      delay(500);
    } else {
      initialized = true;
    }
  }
  SERIAL_PORT.println(F("Device connected!"));

  bool success = true; // Use success to show if the DMP configuration was successful

  // Initialize the DMP. initializeDMP is a weak function. You can overwrite it if you want to e.g. to change the sample
  // rate
  success &= (myICM.initializeDMP() == ICM_20948_Stat_Ok);

  // Enable the DMP orientation sensor
  success &= (myICM.enableDMPSensor(INV_ICM20948_SENSOR_ORIENTATION) == ICM_20948_Stat_Ok);

  // Configuring DMP to output data at multiple ODRs:
  // DMP is capable of outputting multiple sensor data at different rates to FIFO.
  // Setting value can be calculated as follows:
  // Value = (DMP running rate / ODR ) - 1
  // E.g. For a 5Hz ODR rate when DMP is running at 55Hz, value = (55/5) - 1 = 10.
  success &= (myICM.setDMPODRrate(DMP_ODR_Reg_Quat9, 0) == ICM_20948_Stat_Ok); // Set to the maximum

  // Enable the FIFO
  success &= (myICM.enableFIFO() == ICM_20948_Stat_Ok);

  // Enable the DMP
  success &= (myICM.enableDMP() == ICM_20948_Stat_Ok);

  // Reset DMP
  success &= (myICM.resetDMP() == ICM_20948_Stat_Ok);

  // Reset FIFO
  success &= (myICM.resetFIFO() == ICM_20948_Stat_Ok);

  // Check success
  if (success) {
    SERIAL_PORT.println(F("DMP enabled!"));
  } else {
    SERIAL_PORT.println(F("Enable DMP failed!"));
    while (1)
      ; // Do nothing more
  }
}

bool hasCalibrated = false;
Eigen::Vector3d firstEulerAngles;

void loop() {
  icm_20948_DMP_data_t data;
  myICM.readDMPdataFromFIFO(&data);
  if ((myICM.status == ICM_20948_Stat_Ok) || (myICM.status == ICM_20948_Stat_FIFOMoreDataAvail)) {
    if ((data.header & DMP_header_bitmap_Quat9) > 0) {
      Eigen::Vector3d newEulerAngles = getEulerAngles(data);
      if (hasCalibrated) {
        mouse.move(saturate_cast<signed char>((newEulerAngles[0] - firstEulerAngles[0]) * 100),
                   saturate_cast<signed char>((newEulerAngles[1] - firstEulerAngles[1]) * 100));
      } else {
        Serial.println("Calibrating! Move the mouse to the \"forward\" position by the end of 5 seconds.");
        delay(5000);
        hasCalibrated = true;
        firstEulerAngles = newEulerAngles;
      }
    }
  }
  if (myICM.status != ICM_20948_Stat_FIFOMoreDataAvail) {
    delay(10);
  }
}