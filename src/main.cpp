#include <Arduino.h>
#include <Wire.h>
#include <TFT_eSPI.h>
#include <esp32/rom/spi_flash.h>
#include "display.h"
#include "io.h"

#define ADC_ENABLE_PIN 14
#define UP_BUTTON_PIN 35
#define DOWN_BUTTON_PIN 0

xQueueHandle navigationEvents = xQueueCreate(4, sizeof(pageEvent_t));

TFT_eSPI tftDisplay = TFT_eSPI(); //initialize T-display
TFT_eSprite bufferA = TFT_eSprite(&tftDisplay);
TFT_eSprite bufferB = TFT_eSprite(&tftDisplay);
Display display(&tftDisplay, &bufferA, &bufferB);

DisplayManager displayManager(&display);

uint32_t frame = 0;

// Button instantiation
Button<0> upButton(displayManager.eventQueue, pageEvent_t::NAV_PRESS, pageEvent_t::NAV_DOWN, pageEvent_t::NAV_SELECT);
Button<35> downButton(displayManager.eventQueue, pageEvent_t::NAV_PRESS, pageEvent_t::NAV_UP, pageEvent_t::NAV_CANCEL);

class BlankPage : public DisplayPage {
public:
  BlankPage(Display* display, DisplayManager* displayManager, const char* pageName) : DisplayPage(display, displayManager, pageName) {}
  void draw() {
    display->textFormat(2, TFT_WHITE);
    display->drawString(pageName, 30,30);
    display->drawNavArrow(120, 80, pageName[12]&1, 0.5 - 0.5*cos(6.28318*float(frameCounter%90)/90.0), 0x461F, TFT_BLACK);
    frameCounter++;
  };
  void onEvent(pageEvent_t event) {
    if (event == pageEvent_t::NAV_CANCEL) this->displayManager->pageStack.pop();
  };
};

BlankPage myPlaceholderA(&display, &displayManager, "Placeholder A");
BlankPage myPlaceholderB(&display, &displayManager, "Placeholder B");
BlankPage myPlaceholderC(&display, &displayManager, "Placeholder C");
BlankPage myPlaceholderD(&display, &displayManager, "Placeholder D");
BlankPage myPlaceholderE(&display, &displayManager, "Placeholder E");
MenuPage mainMenuPage(&display, &displayManager, "Main Menu", &myPlaceholderA, &myPlaceholderB, &myPlaceholderC, &myPlaceholderD, &myPlaceholderE);
HomePage homepage(&display, &displayManager, "Home Page", &mainMenuPage);

int16_t getBatteryPercentage() {
  digitalWrite(ADC_ENABLE_PIN, HIGH);
  uint16_t v1 = analogRead(34);
  digitalWrite(ADC_ENABLE_PIN, LOW);

  float battery_voltage = ((float)v1 / 4095.0) * 2.0 * 3.3 * (1100 / 1000.0);
  return max(0, min(100, int((battery_voltage-3.2) * 100)));
}

TaskHandle_t drawTaskHandle;
void drawTask (void * pvParameters) {
  TickType_t lastWakeTime = xTaskGetTickCount();

  while (true) {
    display.clear();
    displayManager.draw();

    display.drawLine(210, 40, 210 + 10 * cos(frame / 10.0), 40 + 10 * sin(frame / 10.0));

    display.pushChanges();
    display.dim(127 + 127 * cos(frame / 30.0));
    frame++;
    vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(33));
  }
}

void setup() {
  // put your setup code here, to run once:
  display.begin();
  display.setStroke(TFT_CYAN);

  displayManager.setHomepage(&homepage);

  //starts Serial Monitor
  Serial.begin(115200);
  pinMode(ADC_ENABLE_PIN, OUTPUT);

  // Attach button interrupts
  upButton.attach();
  downButton.attach();

  uint32_t id = g_rom_flashchip.device_id;
  id = ((id & 0xff) << 16) | ((id >> 16) & 0xff) | (id & 0xff00);
  id = (id >> 16) & 0xFF;
  Serial.printf("Flash size is %i\n", 2 << (id - 1));

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

void loop() {
  // put your main code here, to run repeatedly:

 }