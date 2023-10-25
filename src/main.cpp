#include <Arduino.h>
#include <Adafruit_MPU6050.h>
#include <Wire.h>
#include <BleMouse.h>

#define BUTTON_PIN 14 //Which pin is the mouse click button connected to?
#define AVG_SIZE 20 //How many inputs will we keep in rolling average array?

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


inline int sign(float inVal) {
  /*Returns 1 if input is greater than 0, 0 if input is 0, or -1 if input is less than 0*/
  return (inVal > 0) - (inVal < 0);
}

class RollingAverage {
/*Used to keep track of past inputed values to try to smooth movement*/
private:
  float avg[AVG_SIZE];
  bool isInit = false;  // Is the avg buffer full of samples?
  uint8_t head = 0;     // Index of most recent value
  float avgVal;
  uint8_t dStable = 0;  // How stable the direction of the movement is

public:
  RollingAverage () {}
  ~RollingAverage () {}

  void update(float val) {
    /*Update Rolling array with input*/
    if (isInit) {
      this->avg[++this->head%=AVG_SIZE] = val;
      float sum = 0;
      int dir = 0;
      for (uint8_t i=(this->head+1)%AVG_SIZE; i!=this->head; ++i%=AVG_SIZE) {
        sum += this->avg[i];
        dir += sign(this->avg[(i+1)%AVG_SIZE]-this->avg[i]);
      }
      this->avgVal = sum / AVG_SIZE;
      this->dStable = abs(dir);
    }
    else {
      for (uint8_t i=0; i<AVG_SIZE; i++) this->avg[++this->head%=AVG_SIZE] = val;
      this->avgVal = val;
      isInit = true;
    }
  }

  uint8_t stability() {
    /*Returns how consistant is the recent input with eachother?*/
    return this->dStable;
  }

  float get() {
    /*Returns the average value of recent inputs*/
    return this->avgVal;
  }
};

volatile bool buttonPress = false;//Is the button currently pressed?

//Initialize values to be smoothed later on
RollingAverage roll;
RollingAverage pitch;
RollingAverage gRoll;
RollingAverage gPitch;


//set previous inputs to 0
float oldRoll = 0;
float oldPitch = 0;

//sets up a function that can interupt and immediately be ran
void IRAM_ATTR onButtonPress() {
  buttonPress = true;
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
  // put your setup code here, to run once:
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(BUTTON_PIN, onButtonPress, FALLING);//when Button_pin goes from High to low (button is released) onButtonPress is ran

  //starts Serial Monitor
  Serial.begin(115200);


  if (!mpu.begin()) {//if MPU can't connect
    Serial.println("Could not find MPU");
    while(1);
  }

  Serial.println("Found MPU6050");
  Serial.println("Starting Bluetooth mouse");
  mouse.begin();//starts Bluetooth mouse
}

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
    else Serial.println("Mouse not connected!");
    buttonPress = false;
  }
#endif
}
