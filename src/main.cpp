#include <Arduino.h>
#include <LittleFS.h>
#include <FS.h>
#include <Wire.h>
#include <TFT_eSPI.h>
#include <esp32/rom/spi_flash.h>
#include "display.h"
#include "io.h"
#include "power.h"

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

// Define a blank placeholder page
class BlankPage : public DisplayPage {
public:
  BlankPage(Display* display, DisplayManager* displayManager, const char* pageName) : DisplayPage(display, displayManager, pageName) {}
  void draw() {
    display->textFormat(2, TFT_WHITE);
    display->drawString(pageName, 30, 30);
    display->drawString(String(touchRead(T7)), 30, 60);
    display->drawNavArrow(120, 110, pageName[12]&1, 0.5 - 0.5*cos(6.28318*float(frameCounter%90)/90.0), 0x461F, TFT_BLACK);
    frameCounter++;
  };
  void onEvent(pageEvent_t event) {
    if (event == pageEvent_t::NAV_CANCEL) this->displayManager->pageStack.pop();
  };
};

// Instantiate display page hierarchy
BlankPage myPlaceholderA(&display, &displayManager, "Placeholder A");
BlankPage myPlaceholderB(&display, &displayManager, "Placeholder B");
BlankPage myPlaceholderC(&display, &displayManager, "Placeholder C");
BlankPage myPlaceholderD(&display, &displayManager, "Placeholder D");
BlankPage myPlaceholderE(&display, &displayManager, "Placeholder E");
MenuPage mainMenuPage(&display, &displayManager, "Main Menu", &myPlaceholderA, &myPlaceholderB, &myPlaceholderC, &myPlaceholderD, &myPlaceholderE);
HomePage homepage(&display, &displayManager, "Home Page", &mainMenuPage);

// Use the ADC to read the battery voltage - convert result to a percentage
int16_t getBatteryPercentage() {
  digitalWrite(ADC_ENABLE_PIN, HIGH);
  vTaskDelay(pdMS_TO_TICKS(10));
  uint16_t v1 = analogRead(34);
  digitalWrite(ADC_ENABLE_PIN, LOW);

  float battery_voltage = ((float)v1 / 4095.0) * 2.0 * 3.3 * (1100 / 1000.0);
  return max(0, min(100, int((battery_voltage-3.2) * 100)));
}

// Define the display drawing task and a place to store its handle
TaskHandle_t drawTaskHandle;
void drawTask (void * pvParameters) {
  display.drawBitmapSPIFFS("/splash.bmp", 80, 15);  // Splash screen on startup
  display.pushChanges();                            // Definitely don't forget how your own wrapper class works

  vTaskDelay(pdMS_TO_TICKS(2000));                  // Keep splishin' and splashin' for 2 seconds

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

  // Initialize LittleFS
  if (!LittleFS.begin()) {
    LittleFS.begin(true);                                                       // Format the filesystem if it failed to mount
    Serial.println("SPIFFS had to be formatted before mounting - data lost.");  // This can happen on the first upload or when the partition scheme is changed
  }                                                                             // Just reupload the filesystem image - this is different from uploading the program
  else {
    Serial.println("LittleFS Tree");  // Directory listing
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

  // If we just woke up from deep sleep, don't attach the buttons until the user lets go
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_ULP) {
    stopULP();
    pinMode(DOWN_BUTTON_PIN, INPUT_PULLUP);
    while (!digitalRead(DOWN_BUTTON_PIN));
  }

  // Attach button interrupts
  upButton.attach();
  downButton.attach();
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