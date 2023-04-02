#include <Arduino.h>
#include <Wire.h>
#include <TFT_eSPI.h>
#include <esp32/rom/spi_flash.h>
#include <ulp_common.h>
#include "display.h"
#include "io.h"
#include "imgs\hand.h"
#include "imgs\pointer.h"
#include "imgs\middle.h"
#include "imgs\ring.h"
#include "imgs\pinky.h"
#include "imgs\thumb.h"
#include "imgs\scrollUp.h"
#include "imgs\scrollDown.h"

// Pin definitions
#define ADC_ENABLE_PIN 14
#define UP_BUTTON_PIN 35
#define DOWN_BUTTON_PIN 0
#define LMB_TOUCH_CHANNEL 7
#define RMB_TOUCH_CHANNEL 5

// Create event queues for inter-process/ISR communication
xQueueHandle navigationEvents = xQueueCreate(4, sizeof(pageEvent_t));
xQueueHandle mouseEvents = xQueueCreate(4, sizeof(mouseEvent_t));

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
Button upButton(0, displayManager.eventQueue, pageEvent_t::NAV_PRESS, pageEvent_t::NAV_DOWN, pageEvent_t::NAV_SELECT);
Button downButton(35, displayManager.eventQueue, pageEvent_t::NAV_PRESS, pageEvent_t::NAV_UP, pageEvent_t::NAV_CANCEL);

// Touch button instantiation
TouchPadInstance lMouseButton = TouchPad(LMB_TOUCH_CHANNEL, mouseEvents, mouseEvent_t::LMB_PRESS, mouseEvent_t::LMB_RELEASE);
TouchPadInstance rMouseButton = TouchPad(RMB_TOUCH_CHANNEL, mouseEvents, mouseEvent_t::RMB_PRESS, mouseEvent_t::RMB_RELEASE);

class BlankPage : public DisplayPage {
    public:
    BlankPage(Display* display, DisplayManager* displayManager, const char* pageName);

    void draw();
    void onEvent(pageEvent_t event);
};

class inputDisplay : public DisplayPage {
    bool lmb;
    bool rmb;
    bool scrollU;
    bool scrollD;
public:
    inputDisplay(Display* display, DisplayManager* displayManager, const char* pageName);

    void draw();
    void onEvent(pageEvent_t event);
    void onMouseEvent(mouseEvent_t event);
};

// Define a blank placeholder page
BlankPage::BlankPage(Display* display, DisplayManager* displayManager, const char* pageName) : DisplayPage(display, displayManager, pageName) {}
void BlankPage::draw() {
    display->textFormat(2, TFT_WHITE);
    display->drawString(pageName, 30, 30);
    display->drawString(String(touchRead(T7)), 30, 60);
    display->drawNavArrow(120, 110, pageName[12]&1, 0.5 - 0.5*cos(6.28318*float(frameCounter%90)/90.0), 0x461F, TFT_BLACK);
    frameCounter++;
};
void BlankPage::onEvent(pageEvent_t event) {
    if (event == pageEvent_t::NAV_CANCEL) this->displayManager->pageStack.pop();
};

inputDisplay::inputDisplay(Display* display, DisplayManager* displayManager, const char* pageName) : DisplayPage(display, displayManager, pageName) {}
void inputDisplay::draw() {
    display->textFormat(2, TFT_WHITE);
    display->drawString(pageName, 30, 30);
    display->pushImage(60,60, 64, 64, hand);
};
void inputDisplay::onEvent(pageEvent_t event) {
    if (event == pageEvent_t::NAV_CANCEL) this->displayManager->pageStack.pop();
}

//declared on Mouse event that is relevant to the input display
void inputDisplay::onMouseEvent(mouseEvent_t event) {
    switch(event) {
        case mouseEvent_t::LMB_PRESS:
            lmb = true;
            break;
        case mouseEvent_t::LMB_RELEASE:
            lmb = false;
            break;
        case mouseEvent_t::RMB_PRESS:
            rmb = true;
            break;
        case mouseEvent_t::RMB_RELEASE:
            lmb = false;
            break;
        case mouseEvent_t::SCROLL_UP:
            scrollU = true;
            break;
        case mouseEvent_t::SCROLL_DOWN:
            scrollD = true;
            break;
        default:
            break;
    }
    if(lmb) {
        display->pushImage(60,60,64,64, pointer);
    }
    if(rmb) {
        display->pushImage(60,60,64,64, middle);
    }
    if(scrollUp) {
        display->pushImage(60,60,64,64, scrollUp);
    }
    if(scrollDown) {
        display->pushImage(60,60,64,64, scrollDown);
    }
    //configure the rest of the input here
}


// Instantiate display page hierarchy
inputDisplay myPlaceholderA(&display, &displayManager, "input");
BlankPage myPlaceholderB(&display, &displayManager, "Placeholder B");
BlankPage myPlaceholderC(&display, &displayManager, "Placeholder C");
BlankPage myPlaceholderD(&display, &displayManager, "Placeholder D");
BlankPage myPlaceholderE(&display, &displayManager, "Placeholder E");
MenuPage mainMenuPage(&display, &displayManager, "Main Menu", &myPlaceholderA, &myPlaceholderB, &myPlaceholderC, &myPlaceholderD, &myPlaceholderE);
HomePage homepage(&display, &displayManager, "Home Page", &mainMenuPage);

// Use the ADC to read the battery voltage - convert result to a percentage
int16_t getBatteryPercentage() {
  digitalWrite(ADC_ENABLE_PIN, HIGH);
  uint16_t v1 = analogRead(34);
  digitalWrite(ADC_ENABLE_PIN, LOW);

  float battery_voltage = ((float)v1 / 4095.0) * 2.0 * 3.3 * (1100 / 1000.0);
  return max(0, min(100, int((battery_voltage-3.2) * 100)));
}

// Define the display drawing task and a place to store its handle
TaskHandle_t drawTaskHandle;
void drawTask (void * pvParameters) {
  TickType_t lastWakeTime = xTaskGetTickCount();

  uint32_t frame = 0;

  while (true) {
    display.clear();
    displayManager.draw();

    display.drawLine(210, 40, 210 + 10 * cos(frame / 10.0), 40 + 10 * sin(frame / 10.0));

    display.pushChanges();
    frame++;
    vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(33));
  }
}

void setup() {
  // put your setup code here, to run once:
  // Start UART transceiver
  Serial.begin(115200);

  // Configure battery voltage reading pin
  pinMode(ADC_ENABLE_PIN, OUTPUT);

  // Attach button interrupts
  upButton.attach();
  downButton.attach();

  // Attach touch pad interrupts
  attachTouchPads();

  // Initialize display
  display.begin();
  display.setStroke(TFT_CYAN);

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
    drawTask,         // Task code is in the drawTask() function
    "Draw Task",      // Descriptive task name
    4000,             // Stack depth
    NULL,             // Parameter to function (unnecessary here)
    1,                // Task priority
    &drawTaskHandle,  // Variable to hold new task handle
    1                 // Pin the task to the core that doesn't handle WiFi/Bluetooth
  );
}

// I'm lazy
#define DO_CASE(c) case mouseEvent_t::c : Serial.println(#c); break

void loop() {
  // put your main code here, to run repeatedly:
  // Relay test messages from touch pads to Serial
  if (uxQueueMessagesWaiting(mouseEvents)) {
    mouseEvent_t messageReceived;
    xQueueReceive(mouseEvents, &messageReceived, 0);
    switch (messageReceived) {
      DO_CASE(LMB_PRESS);
      DO_CASE(LMB_RELEASE);
      DO_CASE(RMB_PRESS);
      DO_CASE(RMB_RELEASE);
      default:
        Serial.println("I dunno man...");
        break;
    }
  }
}