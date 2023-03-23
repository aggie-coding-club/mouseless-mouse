#include <Arduino.h>
#include <Wire.h>
#include <TFT_eSPI.h>
#include "display.h"

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

TaskHandle_t drawTaskHandle;
void drawTask (void * pvParameters) {
  TickType_t lastWakeTime = xTaskGetTickCount();
  while (true) {
    display.clear();
    display.textFormat(2, TFT_WHITE);
    display.drawString("Battery Life:", 30,30);
    display.textFormat(3, TFT_WHITE);
    display.drawString(String(getBatteryPercentage()) + "%", 30, 50);

    display.drawLine(200, 20, 200 + 10 * cos(frame / 10.0), 20 + 10 * sin(frame / 10.0));

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
  pinMode(14, OUTPUT);

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