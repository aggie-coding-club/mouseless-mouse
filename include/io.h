#ifndef IO_HANDLERS
#define IO_HANDLERS

#include <Arduino.h>
#include "display.h"

#define DEBOUNCE_TIME 50
#define LONGPRESS_TIME 300

class Button {
private:
  static void buttonISR(void* instancePtr);
  static void finalDebounce(xTimerHandle timer);

public:
  byte pin;
  xQueueHandle eventQueue;
  pageEvent_t pressEvent;
  pageEvent_t bumpEvent;
  pageEvent_t holdEvent;
  xTimerHandle debounceTimer;
  bool isPressed;
  uint32_t pressTimestamp;    // Timestamp of the last time the button was pressed
  uint32_t releaseTimestamp;  // Timestamp of the last time the button was released
  uint32_t stateChangeTimestamp;

  Button(byte pin, xQueueHandle eventQueue, pageEvent_t pressEvent, pageEvent_t bumpEvent, pageEvent_t holdEvent);
  void attach();
};

#endif