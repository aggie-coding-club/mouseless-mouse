#include <Arduino.h>
#include "display.h"
#include "io.h"

// Create a Button that sends page events to a queue
Button::Button(byte pin, xQueueHandle eventQueue, pageEvent_t pressEvent, pageEvent_t bumpEvent, pageEvent_t holdEvent) :
    pin(pin),
    eventQueue(eventQueue),
    pressEvent(pressEvent),
    bumpEvent(bumpEvent),
    holdEvent(holdEvent),
    isPressed(false),
    pressTimestamp(0),
    releaseTimestamp(0),
    stateChangeTimestamp(0)
{
    debounceTimer = xTimerCreate(
        "Button Debounce Callback",     // Timer name
        pdMS_TO_TICKS(DEBOUNCE_TIME),   // Timer duration
        pdFALSE,                        // Don't auto-restart timer on expiry
        this,                           // Pointer to instance can be retrieved by static callback function
        &Button::finalDebounce
    );
}

// Change the button state and start a timer to check back when the button has stabilized
void Button::buttonISR(void* instancePtr) {
    Button* instance = reinterpret_cast<Button*>(instancePtr);
    pageEvent_t eventToSend;
    BaseType_t wokeTask = pdFALSE;      // Will be set to pdTRUE if a higher-priority task was unblocked by queueing an event
    if (!digitalRead(instance->pin)) {  // Pin is pulled up, so pressing the button creates a falling edge
        eventToSend = instance->pressEvent;
        if (millis() - instance->stateChangeTimestamp > DEBOUNCE_TIME) {
            instance->pressTimestamp = millis();
            instance->stateChangeTimestamp = instance->pressTimestamp;
            instance->isPressed = true;
            xQueueSendFromISR(instance->eventQueue, &eventToSend, &wokeTask);
        }
    }
    else {
        if (millis() - instance->stateChangeTimestamp > LONGPRESS_TIME)
            eventToSend = instance->holdEvent;
        else
            eventToSend = instance->bumpEvent;
        if (millis() - instance->stateChangeTimestamp > DEBOUNCE_TIME) {
            instance->releaseTimestamp = millis();
            instance->stateChangeTimestamp = instance->releaseTimestamp;
            instance->isPressed = false;
            xQueueSendFromISR(instance->eventQueue, &eventToSend, &wokeTask);
        }
    }
    xTimerResetFromISR(instance->debounceTimer, &wokeTask);
    detachInterrupt(instance->pin);     // Only interrupt once on the very first state change, then use a timer to check the pin again
    portYIELD_FROM_ISR(wokeTask);
}

// Called regularly until the button state has stabilized, at which point the interrupt is re-enabled
void Button::finalDebounce(xTimerHandle timer) {
    Button* instance = reinterpret_cast<Button*>(pvTimerGetTimerID(timer));
    bool pinState = !digitalRead(instance->pin);
    pageEvent_t eventToSend;
    if (pinState == instance->isPressed) {
        attachInterruptArg(instance->pin, &Button::buttonISR, instance, CHANGE);
        return;  // The final state set by the actual ISR was correct
    }
    // If the pin state has not changed, execution will not reach here
    instance->stateChangeTimestamp = millis();
    if (pinState == true) {
        eventToSend = instance->pressEvent;
        instance->pressTimestamp = instance->stateChangeTimestamp;
        instance->isPressed = true;
        xQueueSend(instance->eventQueue, &eventToSend, 0);
    }
    else {
        if (millis() - instance->pressTimestamp > LONGPRESS_TIME)
            eventToSend = instance->holdEvent;
        else
            eventToSend = instance->bumpEvent;
        instance->releaseTimestamp = instance->stateChangeTimestamp;
        instance->isPressed = false;
        xQueueSend(instance->eventQueue, &eventToSend, 0);
    }
    xTimerReset(timer, 0);
}

// Attach the Button instance to its IO pin - must be called within void setup() or void loop()
void Button::attach() {
    pinMode(pin, INPUT_PULLUP);
    attachInterruptArg(pin, &Button::buttonISR, this, CHANGE);
}

// Detach a Button instance from its IO pin
void Button::detach() {
    detachInterrupt(pin);
    pinMode(pin, INPUT);
}



// One timer to rule them all
xTimerHandle TouchPadInstance::pollTimer = xTimerCreate(
    "Touch Polling",        // Timer name
    pdMS_TO_TICKS(5),       // Duration
    pdTRUE,                 // Auto-reload on expiry
    touchInstanceMap,       // This value can be retrieved by the callback function
    &TouchPadInstance::touchPoll
);

// Don't call this directly - use the TouchPad(n, q, p, r) macro
TouchPadInstance::TouchPadInstance(byte touchPadNum, xQueueHandle eventQueue, mouseEvent_t pressEvent, mouseEvent_t releaseEvent, byte pin) :
    pin(pin),
    touchThreshold(TOUCH_DEFAULT_THRESHOLD),
    eventQueue(eventQueue),
    pressEvent(pressEvent),
    releaseEvent(releaseEvent)
{
    touchInstanceMap[touchPadNum] = this;
}

// If you have multiple instances, just call attachTouchPads() instead
void TouchPadInstance::attach() {
    touchAttachInterruptArg(pin, &TouchPadInstance::touchISR, this, touchThreshold);
}

// Set the touch sensitivity of this pad, overriding the default sensitivity
void TouchPadInstance::setThreshold(uint16_t touchThreshold) {
    this->touchThreshold = touchThreshold;
}

// Remap the mouse events fired by a particular touch pad. Returns false if mapping was unsuccessful (because the pad was "pressed")
bool TouchPadInstance::remapEvents(mouseEvent_t pressEvent, mouseEvent_t releaseEvent) {
    if (this->isPressed) return false;  // If we remap events while a pad is "pressed", one of the mouse buttons will probably get stuck
    this->pressEvent = pressEvent;
    this->releaseEvent = releaseEvent;
    return true;
}

// Call attach() on all TouchPadInstance instances - must be called within void setup() or void loop()
void attachTouchPads() {
    touchInterruptSetThresholdDirection(TOUCH_TRIGGER_BELOW);
    for (int i=0; i < 10; i++) {
        if (touchInstanceMap[i])
            touchInstanceMap[i]->attach();
    }
}

// Send a mouse event and start the touch polling loop
void TouchPadInstance::touchISR(void* instancePtr) {
    TouchPadInstance* instance = reinterpret_cast<TouchPadInstance*>(instancePtr);
    if (touchRead(instance->pin) >= instance->touchThreshold) return;   // Interrupts might be colliding - here's a hotfix!
    instance->isPressed = true;
    mouseEvent_t eventToSend = instance->pressEvent;
    BaseType_t taskWoken = pdFALSE;
    touchDetachInterrupt(instance->pin);
    xQueueSendFromISR(instance->eventQueue, &eventToSend, &taskWoken);
    xTimerResetFromISR(instance->pollTimer, &taskWoken);
    portYIELD_FROM_ISR(taskWoken);
}

// Called regularly until no touch pads are "pressed"
void TouchPadInstance::touchPoll(xTimerHandle timer) {
    bool atLeastOnePressed = false;
    for (int i=0; i < 10; i++) {
        if (!touchInstanceMap[i]) continue; // Jumping execution to null pointers is bad
        TouchPadInstance* instance = reinterpret_cast<TouchPadInstance*>(touchInstanceMap[i]);
        if (instance->isPressed && touchRead(instance->pin) >= instance->touchThreshold + TOUCH_HYSTERESIS) {   // Touch pad is no longer pressed
            instance->isPressed = false;
            mouseEvent_t eventToSend = instance->releaseEvent;
            xQueueSend(instance->eventQueue, &eventToSend, 0);
            instance->attach();
        }
        else atLeastOnePressed = true;
    }
    if (!atLeastOnePressed)
        xTimerStop(timer, 0);
}