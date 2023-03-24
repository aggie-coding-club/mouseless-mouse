#include <Arduino.h>
#include <Wire.h>
#include <TFT_eSPI.h>
#include <esp32/rom/spi_flash.h>
#include "display.h"

#define UP_BUTTON_PIN 35
#define DOWN_BUTTON_PIN 0

TFT_eSPI tft = TFT_eSPI(); //initialize T-display
TFT_eSprite bufferA = TFT_eSprite(&tft);
TFT_eSprite bufferB = TFT_eSprite(&tft);
Display display = Display(&tft, &bufferA, &bufferB);

uint32_t frame = 0;

int16_t getBatteryPercentage() {
  digitalWrite(14, HIGH);
  uint16_t v1 = analogRead(34);
  digitalWrite(14, LOW);

  float battery_voltage = ((float)v1 / 4095.0) * 2.0 * 3.3 * (1100 / 1000.0);
  return max(0, min(100, int((battery_voltage-3.2) * 100)));
}

class BlankPage : public DisplayPage {
public:
  BlankPage(Display* display, xQueueHandle eventQueue, const char* pageName) : DisplayPage(display, eventQueue, pageName) {}
  void draw() {
    display->textFormat(2, TFT_WHITE);
    display->drawString(pageName, 30,30);
  };
  void onEvent(pageEvent_t event) {};
};

xQueueHandle navigationEvents = xQueueCreate(4, sizeof(pageEvent_t));
BlankPage myPlaceholderA(&display, navigationEvents, "Placeholder A");
BlankPage myPlaceholderB(&display, navigationEvents, "Placeholder B");
MenuPage mainMenuPage(&display, navigationEvents, "Main Menu", &myPlaceholderA, &myPlaceholderB);
HomePage homepage(&display, navigationEvents, "Home Page", &mainMenuPage);

TaskHandle_t drawTaskHandle;
void drawTask (void * pvParameters) {
  TickType_t lastWakeTime = xTaskGetTickCount();

  pageEvent_t testEvent = pageEvent_t::NAV_SELECT;
  xQueueSend(navigationEvents, &testEvent, 0);
  // testEvent = pageEvent_t::NAV_DOWN;
  // xQueueSend(navigationEvents, &testEvent, 0);
  // testEvent = pageEvent_t::NAV_SELECT;
  // xQueueSend(navigationEvents, &testEvent, 0);

  while (true) {
    display.clear();
    homepage.draw();

    display.drawLine(210, 40, 210 + 10 * cos(frame / 10.0), 40 + 10 * sin(frame / 10.0));

    display.pushChanges();
    display.dim(127 + 127 * cos(frame / 30.0));
    frame++;
    vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(33));
  }
}

void IRAM_ATTR upButtonHandler() {
  pageEvent_t upEvent = pageEvent_t::NAV_UP;
  BaseType_t dunno;
  xQueueSendFromISR(navigationEvents, &upEvent, &dunno);
}

void IRAM_ATTR downButtonHandler() {
  pageEvent_t downEvent = pageEvent_t::NAV_DOWN;
  BaseType_t dunno;
  xQueueSendFromISR(navigationEvents, &downEvent, &dunno);
}

void setup() {
  // put your setup code here, to run once:
  display.begin();
  display.setStroke(TFT_CYAN);

  //starts Serial Monitor
  Serial.begin(115200);
  pinMode(14, OUTPUT);
  pinMode(UP_BUTTON_PIN, INPUT_PULLUP);
  pinMode(DOWN_BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(UP_BUTTON_PIN, upButtonHandler, FALLING);
  attachInterrupt(DOWN_BUTTON_PIN, downButtonHandler, FALLING);

  uint32_t id = g_rom_flashchip.device_id;
  id = ((id & 0xff) << 16) | ((id >> 16) & 0xff) | (id & 0xff00);
  id = (id >> 16) & 0xFF;
  Serial.printf("Flash size is %i", 2 << (id - 1));

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