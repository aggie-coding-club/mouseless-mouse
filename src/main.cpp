#if 0

#include "errors.hpp"

#include <ArduinoEigen/Eigen/Dense>
#include <ArduinoEigen/Eigen/Geometry>

#include <ArduinoLog.h>
#include <BleMouse.h>
#include <TFT_eSPI.h>

#include <cmath>
#include <limits>

#define ICM_20948_USE_DMP
#include <ICM_20948.h>
TFT_eSPI tft = TFT_eSPI();
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

  Wire.begin();
  Wire.setClock(400000);

  if (icm.begin(Wire, 1) == ICM_20948_Stat_Ok && icm.initializeDMP() == ICM_20948_Stat_Ok &&
      icm.enableDMPSensor(INV_ICM20948_SENSOR_ORIENTATION) == ICM_20948_Stat_Ok &&
      icm.enableFIFO() == ICM_20948_Stat_Ok && icm.setDMPODRrate(DMP_ODR_Reg_Quat9, 0) == ICM_20948_Stat_Ok &&
      icm.enableDMP() == ICM_20948_Stat_Ok && icm.resetFIFO() == ICM_20948_Stat_Ok &&
      icm.resetDMP() == ICM_20948_Stat_Ok) {
    Log.infoln("ICM successfully initialized");
  } else {
    Log.infoln(icm.statusString());
    // const std::string error_message = "ICM initialization failed with error status \"";
    // mlm::errors::doFatalError((error_message + icm.statusString() + "\"").c_str(),
    //                           mlm::errors::HARDWARE_INITIALIZATION_FAILED);
  }

  icm_20948_DMP_data_t dataFrame;
  for (;;) {
    icm.readDMPdataFromFIFO(&dataFrame);
    if (icm.status != ICM_20948_Stat_Ok && icm.status != ICM_20948_Stat_FIFOMoreDataAvail) {
      Log.traceln(icm.statusString());
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

#endif
#if 1

/****************************************************************
 * Example6_DMP_Quat9_Orientation.ino
 * ICM 20948 Arduino Library Demo
 * Initialize the DMP based on the TDK InvenSense ICM20948_eMD_nucleo_1.0 example-icm20948
 * Paul Clark, April 25th, 2021
 * Based on original code by:
 * Owen Lyke @ SparkFun Electronics
 * Original Creation Date: April 17 2019
 *
 * ** This example is based on InvenSense's _confidential_ Application Note "Programming Sequence for DMP Hardware
 *Functions".
 * ** We are grateful to InvenSense for sharing this with us.
 *
 * ** Important note: by default the DMP functionality is disabled in the library
 * ** as the DMP firmware takes up 14301 Bytes of program memory.
 * ** To use the DMP, you will need to:
 * ** Edit ICM_20948_C.h
 * ** Uncomment line 29: #define ICM_20948_USE_DMP
 * ** Save changes
 * ** If you are using Windows, you can find ICM_20948_C.h in:
 * ** Documents\Arduino\libraries\SparkFun_ICM-20948_ArduinoLibrary\src\util
 *
 * Please see License.md for the license information.
 *
 * Distributed as-is; no warranty is given.
 ***************************************************************/

// #define QUAT_ANIMATION // Uncomment this line to output data in the correct format for ZaneL's Node.js Quaternion
// animation tool: https://github.com/ZaneL/quaternion_sensor_3d_nodejs
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

[[nodiscard]] Eigen::Vector3d toWorldSpace(const Eigen::Vector3d vec, const icm_20948_DMP_data_t packet) noexcept {
  const double scalingFactor = std::pow(2.0, 30.0);
  const double w = ((double)packet.Quat9.Data.Q1) / scalingFactor;
  const double x = ((double)packet.Quat9.Data.Q2) / scalingFactor;
  const double y = ((double)packet.Quat9.Data.Q3) / scalingFactor;
  const double z = std::sqrt(1.0 - (w * w + x * x + y * y));
  const Eigen::Quaternion<double> quat(w, x, y, z);
  return quat._transformVector(vec);
}

void setup() {

  SERIAL_PORT.begin(115200); // Start the serial console
#ifndef QUAT_ANIMATION
  SERIAL_PORT.println(F("ICM-20948 Example"));
#endif
  tft.init();
  tft.setRotation(1);
  delay(100);

#ifndef QUAT_ANIMATION
  while (SERIAL_PORT.available()) // Make sure the serial RX buffer is empty
    SERIAL_PORT.read();

  SERIAL_PORT.println(F("Press any key to continue..."));

  while (!SERIAL_PORT.available()) // Wait for the user to press a key (send any serial character)
    ;
#endif

#ifdef USE_SPI
  SPI_PORT.begin();
#else
  WIRE_PORT.begin();
  WIRE_PORT.setClock(400000);
#endif

#ifndef QUAT_ANIMATION
  // myICM.enableDebugging(); // Uncomment this line to enable helpful debug messages on Serial
#endif

  bool initialized = false;
  while (!initialized) {

  

    // Initialize the ICM-20948
    // If the DMP is enabled, .begin performs a minimal startup. We need to configure the sample mode etc. manually.
#ifdef USE_SPI
    myICM.begin(CS_PIN, SPI_PORT);
#else
    myICM.begin(WIRE_PORT, AD0_VAL);
#endif

#ifndef QUAT_ANIMATION
    SERIAL_PORT.print(F("Initialization of the sensor returned: "));
    SERIAL_PORT.println(myICM.statusString());
#endif
    if (myICM.status != ICM_20948_Stat_Ok) {
#ifndef QUAT_ANIMATION
      SERIAL_PORT.println(F("Trying again..."));
#endif
      delay(500);
    } else {
      initialized = true;
    }
  }

#ifndef QUAT_ANIMATION
  SERIAL_PORT.println(F("Device connected!"));
#endif

  bool success = true; // Use success to show if the DMP configuration was successful

  // Initialize the DMP. initializeDMP is a weak function. You can overwrite it if you want to e.g. to change the sample
  // rate
  success &= (myICM.initializeDMP() == ICM_20948_Stat_Ok);

  // DMP sensor options are defined in ICM_20948_DMP.h
  //    INV_ICM20948_SENSOR_ACCELEROMETER               (16-bit accel)
  //    INV_ICM20948_SENSOR_GYROSCOPE                   (16-bit gyro + 32-bit calibrated gyro)
  //    INV_ICM20948_SENSOR_RAW_ACCELEROMETER           (16-bit accel)
  //    INV_ICM20948_SENSOR_RAW_GYROSCOPE               (16-bit gyro + 32-bit calibrated gyro)
  //    INV_ICM20948_SENSOR_MAGNETIC_FIELD_UNCALIBRATED (16-bit compass)
  //    INV_ICM20948_SENSOR_GYROSCOPE_UNCALIBRATED      (16-bit gyro)
  //    INV_ICM20948_SENSOR_STEP_DETECTOR               (Pedometer Step Detector)
  //    INV_ICM20948_SENSOR_STEP_COUNTER                (Pedometer Step Detector)
  //    INV_ICM20948_SENSOR_GAME_ROTATION_VECTOR        (32-bit 6-axis quaternion)
  //    INV_ICM20948_SENSOR_ROTATION_VECTOR             (32-bit 9-axis quaternion + heading accuracy)
  //    INV_ICM20948_SENSOR_GEOMAGNETIC_ROTATION_VECTOR (32-bit Geomag RV + heading accuracy)
  //    INV_ICM20948_SENSOR_GEOMAGNETIC_FIELD           (32-bit calibrated compass)
  //    INV_ICM20948_SENSOR_GRAVITY                     (32-bit 6-axis quaternion)
  //    INV_ICM20948_SENSOR_LINEAR_ACCELERATION         (16-bit accel + 32-bit 6-axis quaternion)
  //    INV_ICM20948_SENSOR_ORIENTATION                 (32-bit 9-axis quaternion + heading accuracy)

  // Enable the DMP orientation sensor
  success &= (myICM.enableDMPSensor(INV_ICM20948_SENSOR_ORIENTATION) == ICM_20948_Stat_Ok);

  // Enable any additional sensors / features
  // success &= (myICM.enableDMPSensor(INV_ICM20948_SENSOR_RAW_GYROSCOPE) == ICM_20948_Stat_Ok);
  // success &= (myICM.enableDMPSensor(INV_ICM20948_SENSOR_RAW_ACCELEROMETER) == ICM_20948_Stat_Ok);
  // success &= (myICM.enableDMPSensor(INV_ICM20948_SENSOR_MAGNETIC_FIELD_UNCALIBRATED) == ICM_20948_Stat_Ok);

  // Configuring DMP to output data at multiple ODRs:
  // DMP is capable of outputting multiple sensor data at different rates to FIFO.
  // Setting value can be calculated as follows:
  // Value = (DMP running rate / ODR ) - 1
  // E.g. For a 5Hz ODR rate when DMP is running at 55Hz, value = (55/5) - 1 = 10.
  success &= (myICM.setDMPODRrate(DMP_ODR_Reg_Quat9, 0) == ICM_20948_Stat_Ok); // Set to the maximum
  // success &= (myICM.setDMPODRrate(DMP_ODR_Reg_Accel, 0) == ICM_20948_Stat_Ok); // Set to the maximum
  // success &= (myICM.setDMPODRrate(DMP_ODR_Reg_Gyro, 0) == ICM_20948_Stat_Ok); // Set to the maximum
  // success &= (myICM.setDMPODRrate(DMP_ODR_Reg_Gyro_Calibr, 0) == ICM_20948_Stat_Ok); // Set to the maximum
  // success &= (myICM.setDMPODRrate(DMP_ODR_Reg_Cpass, 0) == ICM_20948_Stat_Ok); // Set to the maximum
  // success &= (myICM.setDMPODRrate(DMP_ODR_Reg_Cpass_Calibr, 0) == ICM_20948_Stat_Ok); // Set to the maximum

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
#ifndef QUAT_ANIMATION
    SERIAL_PORT.println(F("DMP enabled!"));
#endif
  } else {
    SERIAL_PORT.println(F("Enable DMP failed!"));
    SERIAL_PORT.println(
        F("Please check that you have uncommented line 29 (#define ICM_20948_USE_DMP) in ICM_20948_C.h..."));
    while (1)
      ; // Do nothing more
  }
}

void loop() {
  // Read any DMP data waiting in the FIFO
  // Note:
  //    readDMPdataFromFIFO will return ICM_20948_Stat_FIFONoDataAvail if no data is available.
  //    If data is available, readDMPdataFromFIFO will attempt to read _one_ frame of DMP data.
  //    readDMPdataFromFIFO will return ICM_20948_Stat_FIFOIncompleteData if a frame was present but was incomplete
  //    readDMPdataFromFIFO will return ICM_20948_Stat_Ok if a valid frame was read.
  //    readDMPdataFromFIFO will return ICM_20948_Stat_FIFOMoreDataAvail if a valid frame was read _and_ the FIFO
  //    contains more (unread) data.
  icm_20948_DMP_data_t data;
  myICM.readDMPdataFromFIFO(&data);
//Battery life
  // Read battery voltage and calculate battery percentage
  int battery_pin = A4;
  float battery_voltage = analogRead(battery_pin) * 3.3 / 4095 * 2;
  int battery_percentage = (battery_voltage - 3.2) / (4.2 - 3.2) * 100;

  // Display battery percentage on OLED display
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(0, 0);
  tft.drawString("Battery Life:", 30,30);
  tft.setTextSize(3);
  tft.setCursor(0, 20);
  tft.drawString(String(battery_percentage), 30, 50);
  tft.drawString("%", 50, 60);

  delay(1000);
  if ((myICM.status == ICM_20948_Stat_Ok) ||
      (myICM.status == ICM_20948_Stat_FIFOMoreDataAvail)) // Was valid data available?
  {
    // SERIAL_PORT.print(F("Received data! Header: 0x")); // Print the header in HEX so we can see what data is arriving
    // in the FIFO if ( data.header < 0x1000) SERIAL_PORT.print( "0" ); // Pad the zeros if ( data.header < 0x100)
    // SERIAL_PORT.print( "0" ); if ( data.header < 0x10) SERIAL_PORT.print( "0" ); SERIAL_PORT.println( data.header,
    // HEX );

    if ((data.header & DMP_header_bitmap_Quat9) > 0) // We have asked for orientation data so we should receive Quat9
    {
      // Q0 value is computed from this equation: Q0^2 + Q1^2 + Q2^2 + Q3^2 = 1.
      // In case of drift, the sum will not add to 1, therefore, quaternion data need to be corrected with right bias
      // values. The quaternion data is scaled by 2^30.

      // SERIAL_PORT.printf("Quat9 data is: Q1:%ld Q2:%ld Q3:%ld Accuracy:%d\r\n", data.Quat9.Data.Q1,
      // data.Quat9.Data.Q2, data.Quat9.Data.Q3, data.Quat9.Data.Accuracy);

      // Scale to +/- 1
      double q1 = ((double)data.Quat9.Data.Q1) / 1073741824.0; // Convert to double. Divide by 2^30
      double q2 = ((double)data.Quat9.Data.Q2) / 1073741824.0; // Convert to double. Divide by 2^30
      double q3 = ((double)data.Quat9.Data.Q3) / 1073741824.0; // Convert to double. Divide by 2^30
      double q0 = sqrt(1.0 - ((q1 * q1) + (q2 * q2) + (q3 * q3)));

      const Eigen::Vector3d j(0, 1, 0);
      const Eigen::Vector3d transformed = toWorldSpace(j, data);
      Eigen::Vector2<long> diff = screen.project(transformed) - mousePosition;
      mouse.move(saturate_cast<signed char>(diff.x()), saturate_cast<signed char>(diff.y()));

#ifndef QUAT_ANIMATION
      SERIAL_PORT.print(transformed.x(), 3);
      SERIAL_PORT.print(F("i + ("));
      SERIAL_PORT.print(transformed.y(), 3);
      SERIAL_PORT.print(F(")j + ("));
      SERIAL_PORT.print(transformed.z(), 3);
      SERIAL_PORT.println(F(")k"));
      // SERIAL_PORT.print(F("Q0:"));
      // SERIAL_PORT.print(q0, 3);
      // SERIAL_PORT.print(F(" Q1:"));
      // SERIAL_PORT.print(q1, 3);
      // SERIAL_PORT.print(F(" Q2:"));
      // SERIAL_PORT.print(q2, 3);
      // SERIAL_PORT.print(F(" Q3:"));
      // SERIAL_PORT.print(q3, 3);
      // SERIAL_PORT.print(F(" Accuracy:"));
      // SERIAL_PORT.println(data.Quat9.Data.Accuracy);
#else
      // Output the Quaternion data in the format expected by ZaneL's Node.js Quaternion animation tool
      SERIAL_PORT.print(F("{\"quat_w\":"));
      SERIAL_PORT.print(q0, 3);
      SERIAL_PORT.print(F(", \"quat_x\":"));
      SERIAL_PORT.print(q1, 3);
      SERIAL_PORT.print(F(", \"quat_y\":"));
      SERIAL_PORT.print(q2, 3);
      SERIAL_PORT.print(F(", \"quat_z\":"));
      SERIAL_PORT.print(q3, 3);
      SERIAL_PORT.println(F("}"));
#endif
    }
  }
  if (myICM.status !=
      ICM_20948_Stat_FIFOMoreDataAvail) // If more data is available then we should read it right away - and not delay
  {
    delay(10);
  }
}

#endif