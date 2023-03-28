#ifndef IO_HANDLERS
#define IO_HANDLERS

#include <Arduino.h>
#include "display.h"
#include "mouse.h"

/*
 * Button debouncing proceeds as follows:
 *   Interrupt - button changed states
 *     Set new button state according to pin state at interrupt time
 *     Start a timer of duration DEBOUNCE_TIME
 *     Disable the interrupt
 *   Callback - timer expired
 *     Check that the button pin is still in the same state
 *     If so, re-enable the interrupt, and we're done
 *     If not, set new button state and restart the timer
 */

#define DEBOUNCE_TIME 50
#define LONGPRESS_TIME 300

#define TOUCH_DEFAULT_THRESHOLD 40  // Threshold value below which touch counter values are interpreted as touches
#define TOUCH_HYSTERESIS 20         // Once a touch pad is in the "pressed" state, it must go this far above the threshold to be "released"
#define TOUCH_POLL_RATE 5           // When a touch is recorded, the pads will be polled every TOUCH_POLL_RATE ms

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

class TouchPadInstance;

// By default, no TouchPadInstance instances have been defined
static TouchPadInstance* touchInstanceMap[10] = {
  nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr
};

class TouchPadInstance {
private:
  static void touchISR(void* instancePtr);
  static void touchPoll(xTimerHandle timer);
  static xTimerHandle pollTimer;

public:
  byte pin;
  xQueueHandle eventQueue;
  mouseEvent_t pressEvent;
  mouseEvent_t releaseEvent;
  uint16_t touchThreshold;
  bool isPressed;
  uint32_t pressTimestamp;
  uint32_t releaseTimestamp;
  uint32_t stateChangeTimestamp;

  TouchPadInstance(byte touchPadNum, xQueueHandle eventQueue, mouseEvent_t pressEvent, mouseEvent_t releaseEvent, byte pin);
  void attach();
  void setThreshold(uint16_t touchThreshold);
  bool remapEvents(mouseEvent_t pressEvent, mouseEvent_t releaseEvent);
};

void attachTouchPads();

// Helper to allow expansion of n before token pasting
#define INSTANTIATE_TOUCH_PAD(n, q, p, r) TouchPadInstance((n), q, p, r, (T##n))

// Use this to instantiate touch pads because Arduino definitions are stupid
#define TouchPad(n, q, p, r) INSTANTIATE_TOUCH_PAD(n, (q), (p), (r))

#endif