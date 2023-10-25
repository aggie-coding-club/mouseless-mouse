#include <ArduinoEigen/Eigen/Dense>
#include <ArduinoEigen/Eigen/Geometry>

#include <Arduino.h>
#include <BleMouse.h>
#include <FS.h>
#include <LittleFS.h>
#include <TFT_eSPI.h>
#include <Wire.h>
#include <cmath>
#include <cstdint>
#include <esp32/rom/spi_flash.h>

#include "display.h"
#include "io.h"
#include "pages.h"
#include "power.h"

#include "ICM_20948.h"

// Define this if you want to test functionality without an IMU connected
#define NO_SENSOR

// Constants
#define ADC_ENABLE_PIN 14
#define UP_BUTTON_PIN 35
#define DOWN_BUTTON_PIN 0
#define LMB_TOUCH_CHANNEL 7
#define RMB_TOUCH_CHANNEL 5
#define SCROLL_TOUCH_CHANNEL 3
#define LOCK_TOUCH_CHANNEL 4
#define CALIBRATE_TOUCH_CHANNEL 2
constexpr signed char SENSITIVITY = 24;

#ifndef NO_SENSOR
const uint16_t ACCENT_COLOR = 0x461F;                   // TFT_eSPI::color565(64, 192, 255)
#else
const uint16_t ACCENT_COLOR = 0xF000;
#endif

// Mouse logic globals
#ifndef NO_SENSOR
ICM_20948_I2C icm;
#endif

// Bluetooth interface class instance
BleMouse mouse("Mouseless Mouse", "Mouseless Team");

// Vectors to hold position data
Eigen::Vector3f calibratedPosX;
Eigen::Vector3f calibratedPosZ;

// Create event queues for inter-process/ISR communication
xQueueHandle navigationEvents = xQueueCreate(4, sizeof(pageEvent_t));
xQueueHandle mouseEvents = xQueueCreate(4, sizeof(mouseEvent_t));
xQueueHandle mouseQueue = xQueueCreate(4, sizeof(mouseEvent_t));

// Instantiate display module
TFT_eSPI tftDisplay = TFT_eSPI();

// Instantiate two sprites to be used as frame buffers
TFT_eSprite bufferA = TFT_eSprite(&tftDisplay);
TFT_eSprite bufferB = TFT_eSprite(&tftDisplay);

// Wrap display module and frame buffers into Display class object
Display display(&tftDisplay, &bufferA, &bufferB);

// Instantiate display page manager
DisplayManager displayManager(&display);

// Button instantiation
Button upButton(
  0,                          // Pin
  displayManager.eventQueue,  // Event queue
  pageEvent_t::NAV_PRESS,     // Event sent on press
  pageEvent_t::NAV_DOWN,      // Event sent on short release
  pageEvent_t::NAV_SELECT     // Event sent on long release
);
Button downButton(
  35,
  displayManager.eventQueue,
  pageEvent_t::NAV_PRESS,
  pageEvent_t::NAV_UP,
  pageEvent_t::NAV_CANCEL
);

// Touch button instantiation
TouchPadInstance lMouseButton =
  TouchPad(
    LMB_TOUCH_CHANNEL,        // Touch controller channel
    mouseEvents,              // Event queue
    mouseEvent_t::LMB_PRESS,  // Event sent on press
    mouseEvent_t::LMB_RELEASE // Event sent on release
  );
TouchPadInstance rMouseButton =
  TouchPad(
    RMB_TOUCH_CHANNEL,
    mouseEvents,
    mouseEvent_t::RMB_PRESS,
    mouseEvent_t::RMB_RELEASE
  );
TouchPadInstance scrollButton =
  TouchPad(
    SCROLL_TOUCH_CHANNEL,
    mouseEvents,
    mouseEvent_t::SCROLL_PRESS,
    mouseEvent_t::SCROLL_RELEASE
  );
TouchPadInstance lockButton =
  TouchPad(
    LOCK_TOUCH_CHANNEL,
    mouseEvents,
    mouseEvent_t::LOCK_PRESS,
    mouseEvent_t::LOCK_RELEASE
  );
TouchPadInstance calibrateButton =
  TouchPad(
    CALIBRATE_TOUCH_CHANNEL,
    mouseEvents,
    mouseEvent_t::CALIBRATE_PRESS,
    mouseEvent_t::CALIBRATE_RELEASE
  );

char *dummyField = new char[32];

// Instantiate display page hierarchy
InputDisplay inputViewPage(&display, &displayManager, "Input");
DebugPage debugPage(&display, &displayManager, "Debug Page");
KeyboardPage keyboard(&display, &displayManager, "Keyboard");
ConfirmationPage confirm(&display, &displayManager, "Power Off");
MenuPage mainMenuPage(&display, &displayManager, "Main Menu",
  &inputViewPage,
  &debugPage,
  keyboard(dummyField),
  confirm("Are you sure?", deepSleep)
);
HomePage homepage(&display, &displayManager, "Home Page", &mainMenuPage);

// Keep track of which mouse functions are active
bool mouseEnableState = true;
bool scrollEnableState = false;



//Start of Orientation detection



/// @brief Filter to smooth values using a rolling average.
/// @tparam T The type of the values to be smoothed.
/// @tparam bufferLength The number of samples to average over.
template <typename T, std::size_t bufferLength> class RollingAverage {
private:
  std::array<T, bufferLength> mBuffer;
  std::size_t mContentLength = 0;
  std::size_t mCursor = 0;
  T mAverage;

public:
  static_assert(bufferLength != 0);

  /// @brief Pass another value into the rolling average.
  /// @param next The new value.
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
  /// @brief Get the latest value in the rolling average buffer.
  /// @return The rolling average of the last bufferLength values inserted.
  [[nodiscard]] T get() const noexcept { return mAverage; }
};

/// @brief Convert a vector from mouse-space to world-space.
/// @details In mouse-space, +x, +y, and +z are defined in accordance with the accelerometer axes defined in the ICM
/// 20948 datasheet. In world-space, +x is due east, +y is due north, and +z is straight up.
/// @param vec The vector in mouse-space.
/// @param icm A reference to the ICM_20948_I2C object whose orientation data is being used.
/// @return The equivalent of `vec` in world-space.
[[nodiscard]] Eigen::Vector3f mouseSpaceToWorldSpace(Eigen::Vector3f vec, ICM_20948_I2C &icm) noexcept {
  constexpr std::size_t ROLLING_AVG_BUFFER_DEPTH = 8U;
  static RollingAverage<Eigen::Vector3f, ROLLING_AVG_BUFFER_DEPTH> up;
  static RollingAverage<Eigen::Vector3f, ROLLING_AVG_BUFFER_DEPTH> north;
  up.update(Eigen::Vector3f(icm.accX(), icm.accY(), icm.accZ()).normalized());
  north.update(Eigen::Vector3f(icm.magX(), -icm.magY(), -icm.magZ()).normalized());
  Eigen::Vector3f adjusted_north = north.get() - (up.get() * up.get().dot(north.get()));
  adjusted_north.normalize();
  Eigen::Vector3f east = adjusted_north.cross(up.get());
  return Eigen::Vector3f{
    east.dot(vec),
    adjusted_north.dot(vec),
    up.get().dot(vec),
  };
}


//End of Mouse orientation detection






// Use the ADC to read the battery voltage - convert result to a percentage
int16_t getBatteryPercentage() {
  digitalWrite(ADC_ENABLE_PIN, HIGH);
  vTaskDelay(pdMS_TO_TICKS(10));
  uint16_t v1 = analogRead(34);
  digitalWrite(ADC_ENABLE_PIN, LOW);

  float battery_voltage = ((float)v1 / 4095.0) * 2.0 * 3.3 * (1100 / 1000.0);
  return max(0, min(100, int((battery_voltage - 3.2) * 100)));
}

// Define the display drawing task and a place to store its handle
TaskHandle_t drawTaskHandle;
void drawTask(void *pvParameters) {
  display.drawBitmapSPIFFS("/splash.bmp", 80, 15); // Splash screen on startup
  display.pushChanges();                           // Definitely don't forget how your own wrapper class works

  vTaskDelay(pdMS_TO_TICKS(2000)); // Keep splishin' and splashin' for 2 seconds

  TickType_t lastWakeTime = xTaskGetTickCount();
  uint32_t frame = 0;

  for (;;) {
    display.clear();
    displayManager.draw();

    // RIP spinny line, gone but not forgotten
    // display.buffer->drawLine(210, 40, 210 + 10 * cos(frame / 10.0), 40 + 10 * sin(frame / 10.0), TFT_CYAN);
    if (displayManager.upButton->isPressed || displayManager.downButton->isPressed) {
        Button *activeButton =
            displayManager.upButton->isPressed ? displayManager.upButton : displayManager.downButton;
        display.drawNavArrow(210, 40, displayManager.upButton->isPressed,
                              pow(millis() - activeButton->pressTimestamp, 2) / pow(LONGPRESS_TIME, 2), ACCENT_COLOR,
                              SEL_COLOR);
    }

    display.pushChanges();
    frame++;
    vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(33));
  }
}

float normalizeMouseMovement(float axisValue) {
  if (axisValue < -1.0f) {
    return -1.0f;
  } else if (axisValue < -0.8f) {
    return 0.1f * axisValue - 0.9f;
  } else if (axisValue < -0.2f) {
    return 1.5f * axisValue + 0.32f;
  } else if (axisValue < 0.0f) {
    return 0.4f * axisValue;
  } else {
    return -normalizeMouseMovement(-axisValue);
  }
}


//Code to run once on start up

void setup() {
  // Begin serial and logging
  Serial.begin(115200);
  // Log.begin(LOG_LEVEL_VERBOSE, &Serial);
  delay(100);

  // Initialize mouse logic components and calibrate mouse
  Wire.begin();
  Wire.setClock(400000);
  mouse.begin();

#ifndef NO_SENSOR
  for (;;) {
    icm.begin();
    if (icm.status != ICM_20948_Stat_Ok) {
      Serial.printf("ICM initialization failed with error status \"%s\", retrying\n", icm.statusString());
      delay(500);
    } else {
      Serial.printf("ICM initialization successful\n");
      break;
    }
  }
  while (!icm.dataReady()) {
    // do nothing
  }

  while (Serial.available() > 0) {
    Serial.read();
  }

  //Initialize Mouse orientation detection
  icm.getAGMT();
  calibratedPosX = mouseSpaceToWorldSpace(Eigen::Vector3f{1.0f, 0.0f, 0.0f}, icm);
  calibratedPosZ = mouseSpaceToWorldSpace(Eigen::Vector3f{0.0f, 0.0f, 1.0f}, icm);
  Serial.println("Mouse calibrated!");
#endif

  // Initialize LittleFS
  if (!LittleFS.begin()) {
    LittleFS.begin(true); // Format the filesystem if it failed to mount
    // This can happen on the first upload or when the partition scheme is changed
    Serial.println("SPIFFS had to be formatted before mounting - data lost."); 
  } 
  else {
    // Just reupload the filesystem image - this is different from uploading the program
    Serial.println("LittleFS Tree"); // Directory listing
    File root = LittleFS.open("/");
    File file = root.openNextFile();
    while (file) {
      Serial.println(file.name());
      file.close();
      file = root.openNextFile();
    }
  }

  // Configure battery voltage reading pin
  pinMode(ADC_ENABLE_PIN, OUTPUT);

  // Attach touch pad interrupts
  attachTouchPads();

  // Initialize display
  display.begin();

  // Set up display page manager
  displayManager.setHomepage(&homepage);
  displayManager.attachButtons(&upButton, &downButton);

  // Get board flash size and print it to serial
  uint32_t id = g_rom_flashchip.device_id;
  id = ((id & 0xff) << 16) | ((id >> 16) & 0xff) | (id & 0xff00);
  id = (id >> 16) & 0xFF;
  Serial.printf("Flash size is %i\n", 2 << (id - 1));

  // Dispatch the display drawing task
  xTaskCreatePinnedToCore(
    drawTask,        // Task code is in the drawTask() function
    "Draw Task",     // Descriptive task name
    4000,            // Stack depth
    NULL,            // Parameter to function (unnecessary here)
    1,               // Task priority
    &drawTaskHandle, // Variable to hold new task handle
    1                // Pin the task to the core that doesn't handle WiFi/Bluetooth
  );

  // If we just woke up from deep sleep, don't attach the buttons until the user lets go
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_ULP) {
    stopULP();
    pinMode(DOWN_BUTTON_PIN, INPUT_PULLUP);
    while (!digitalRead(DOWN_BUTTON_PIN))
      ;
  }

  // Attach button interrupts
  upButton.attach();
  downButton.attach();
}


//Code to constantly run

void loop() {
  // Relay test messages from touch pads to Serial
  if (uxQueueMessagesWaiting(mouseEvents)) {
    mouseEvent_t messageReceived;
    xQueueReceive(mouseEvents, &messageReceived, 0);
    inputViewPage.onMouseEvent(messageReceived);  // Update input view page
    if (mouseEnableState) {//If there is a button event
      switch (messageReceived) {
      case mouseEvent_t::LMB_PRESS:
        Serial.println("LMB_PRESS");
        mouse.press(MOUSE_LEFT);
        break;
      case mouseEvent_t::LMB_RELEASE:
        Serial.println("LMB_RELEASE");
        mouse.release(MOUSE_LEFT);
        break;
      case mouseEvent_t::RMB_PRESS:
        Serial.println("RMB_PRESS");
        mouse.press(MOUSE_RIGHT);
        break;
      case mouseEvent_t::RMB_RELEASE:
        Serial.println("RMB_RELEASE");
        mouse.release(MOUSE_RIGHT);
        break;
      case mouseEvent_t::LOCK_PRESS:
        Serial.println("DISABLED");
        mouseEnableState = !mouseEnableState;
        break;
      case mouseEvent_t::SCROLL_PRESS:
        Serial.println("SCROLL ENBALED");
        scrollEnableState = true;
        break;
      case mouseEvent_t::SCROLL_RELEASE:
        Serial.println("SCROLL DISBALED");
        scrollEnableState = false;
        break;
      case mouseEvent_t::CALIBRATE_PRESS:
        Serial.println("CALIBRATING...");
#ifndef NO_SENSOR
        icm.getAGMT();
        calibratedPosX = mouseSpaceToWorldSpace(Eigen::Vector3f{1.0f, 0.0f, 0.0f}, icm);
        calibratedPosZ = mouseSpaceToWorldSpace(Eigen::Vector3f{0.0f, 0.0f, 1.0f}, icm);
        Serial.println("Mouse calibrated!");
#endif
        break;
      default:
        break;
      }
    }
    else {  // Check only for this event type if the mouse is not enabled
      if (messageReceived == mouseEvent_t::LOCK_PRESS) {
        Serial.println("ENABLED");
        mouseEnableState = !mouseEnableState;
      }
    }

    xQueueSend(mouseQueue, &messageReceived, 0);
  }
#ifndef NO_SENSOR
  // Move the mouse according to incoming IMU data
  if (icm.dataReady() && mouseEnableState) {
    icm.getAGMT();
    Eigen::Vector3f posY = mouseSpaceToWorldSpace(Eigen::Vector3f{0.0f, 1.0f, 0.0f}, icm);
    signed char xMovement = normalizeMouseMovement(posY.dot(calibratedPosX)) * 3.0f * SENSITIVITY;
    signed char zMovement = normalizeMouseMovement(posY.dot(calibratedPosZ)) * SENSITIVITY;
    if (!scrollEnableState) {
      mouse.move(xMovement, zMovement);
    } else {
      mouse.move(0, 0, xMovement, zMovement);
    }
    delay(30);
  }
#endif
}
