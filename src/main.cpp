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

TFT_eSPI tft = TFT_eSPI(); //initialize T-display
TFT_eSprite bufferA = TFT_eSprite(&tft);
TFT_eSprite bufferB = TFT_eSprite(&tft);
Display display = Display(&tft, &bufferA, &bufferB);

uint32_t frame = 0;

// Button setup initialization
template<>
ButtonStatus Button<0>::status {navigationEvents, pageEvent_t::NAV_DOWN, pageEvent_t::NAV_SELECT};
template<>
ButtonStatus Button<35>::status {navigationEvents, pageEvent_t::NAV_UP, pageEvent_t::NAV_CANCEL};

// Button instantiation
Button<0> upButton;
Button<35> downButton;

class BlankPage : public DisplayPage {
public:
  BlankPage(Display* display, xQueueHandle eventQueue, const char* pageName) : DisplayPage(display, eventQueue, pageName) {}
  void draw() {
    display->textFormat(2, TFT_WHITE);
    display->drawString(pageName, 30,30);
  };
  void onEvent(pageEvent_t event) {};
};

BlankPage myPlaceholderA(&display, navigationEvents, "Placeholder A");
BlankPage myPlaceholderB(&display, navigationEvents, "Placeholder B");
MenuPage mainMenuPage(&display, navigationEvents, "Main Menu", &myPlaceholderA, &myPlaceholderB);
HomePage homepage(&display, navigationEvents, "Home Page", &mainMenuPage);

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
    homepage.draw();

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