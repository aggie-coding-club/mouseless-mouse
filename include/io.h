#ifndef IO_HANDLERS
#define IO_HANDLERS

#include <Arduino.h>
#include "display.h"

#define DEBOUNCE_TIME 20
#define LONGPRESS_TIME 300

struct ButtonStatus {
  byte pin;
  xQueueHandle eventQueue;
  pageEvent_t pressEvent;
  pageEvent_t bumpEvent;
  pageEvent_t holdEvent;
  bool isPressed = false;
  uint32_t pressTimestamp = 0;    // Timestamp of the last time the button was pressed
  uint32_t releaseTimestamp = 0;  // Timestamp of the last time the button was released
};

template <ButtonStatus& B>
void IRAM_ATTR buttonHandler();

template <byte P>
struct Button {
private:
  ButtonStatus tempStatus;

public:
  static ButtonStatus status;

  Button(xQueueHandle eventQueue, pageEvent_t pressEvent, pageEvent_t bumpEvent, pageEvent_t holdEvent);
  void attach();
};

////////////////////////////////////////////////////////////////////////////
// Template functions
////////////////////////////////////////////////////////////////////////////

template <byte P>
Button<P>::Button(xQueueHandle eventQueue, pageEvent_t pressEvent, pageEvent_t bumpEvent, pageEvent_t holdEvent) {
  tempStatus.pin = P;
  tempStatus.eventQueue = eventQueue;
  tempStatus.pressEvent = pressEvent;
  tempStatus.bumpEvent = bumpEvent;
  tempStatus.holdEvent = holdEvent;
}

template <byte P>
void Button<P>::attach() {
  status = tempStatus;
  pinMode(status.pin, INPUT_PULLUP);
  attachInterrupt(status.pin, buttonHandler<status>, CHANGE);
}

template <ButtonStatus& B>
void buttonHandler() {
  pageEvent_t eventToSend;
  BaseType_t ign; // Would be truthy if a higher-priority task was unblocked by queueing an event, but there's no blocking queue code
  if (!digitalRead(B.pin)) {    // Pin is pulled up, so pressing the button creates a falling edge
    B.pressTimestamp = millis();
    B.isPressed = true;
    eventToSend = B.pressEvent;
    if (millis() - B.releaseTimestamp > DEBOUNCE_TIME)
      xQueueSendFromISR(B.eventQueue, &eventToSend, &ign);
  }
  else {
    B.releaseTimestamp = millis();
    B.isPressed = false;
    if (millis() - B.pressTimestamp > LONGPRESS_TIME)
      eventToSend = B.holdEvent;
    else
      eventToSend = B.bumpEvent;
    if (millis() - B.pressTimestamp > DEBOUNCE_TIME)
      xQueueSendFromISR(B.eventQueue, &eventToSend, &ign);
  }
}

// Make sure the static ButtonStatus member for each Button<P> is instantiated or we will get linker errors
template <byte P>
ButtonStatus Button<P>::status {};

#endif