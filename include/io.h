#ifndef IO_HANDLERS
#define IO_HANDLERS

#include <Arduino.h>
#include "display.h"

#define DEBOUNCE_TIME 20
#define LONGPRESS_TIME 300

class Button {
private:
  static void buttonISR(void* instancePtr);
public:
  byte pin;
  xQueueHandle eventQueue;
  pageEvent_t pressEvent;
  pageEvent_t bumpEvent;
  pageEvent_t holdEvent;
  bool isPressed;
  uint32_t pressTimestamp;    // Timestamp of the last time the button was pressed
  uint32_t releaseTimestamp;  // Timestamp of the last time the button was released

  Button(byte pin, xQueueHandle eventQueue, pageEvent_t pressEvent, pageEvent_t bumpEvent, pageEvent_t holdEvent);
  void attach();
};

#endif