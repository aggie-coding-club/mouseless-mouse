#ifndef IO_HANDLERS
#define IO_HANDLERS

#include <Arduino.h>
#include "display.h"

struct ButtonStatus {
  byte pin;
  xQueueHandle eventQueue;
  pageEvent_t bumpEvent;
  pageEvent_t holdEvent;
  bool isPressed = false;
  uint32_t pressTimestamp = 0;  // Timestamp of the last time the button was pressed down

  ButtonStatus(xQueueHandle eventQueue, pageEvent_t bumpEvent, pageEvent_t holdEvent) {
    this->eventQueue = eventQueue;
    this->bumpEvent = bumpEvent;
    this->holdEvent = holdEvent;
  }
};

template <ButtonStatus& B>
void IRAM_ATTR buttonHandler() {
  if (!digitalRead(B.pin)) {    // Pin is pulled up, so pressing the button creates a falling edge
    B.pressTimestamp = millis();
    B.isPressed = true;
  }
  else {
    B.isPressed = false;
    pageEvent_t eventToSend;
    BaseType_t ign; // Would be truthy if a higher-priority task was unblocked by queueing an event, but there's no blocking queue code
    if (millis() - B.pressTimestamp > 500)
      eventToSend = B.holdEvent;
    else
      eventToSend = B.bumpEvent;
    if (millis() - B.pressTimestamp > 50)
      xQueueSendFromISR(B.eventQueue, &eventToSend, &ign);
  }
}

template <byte P>
struct Button {
  static ButtonStatus status;

  Button() {
    status.pin = P;
  }
  void attach() {
    pinMode(status.pin, INPUT_PULLUP);
    attachInterrupt(status.pin, buttonHandler<status>, CHANGE);
  }
};

#endif