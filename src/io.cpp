#include <Arduino.h>
#include "display.h"
#include "io.h"

Button::Button(byte pin, xQueueHandle eventQueue, pageEvent_t pressEvent, pageEvent_t bumpEvent, pageEvent_t holdEvent) :
    pin(pin),
    eventQueue(eventQueue),
    pressEvent(pressEvent),
    bumpEvent(bumpEvent),
    holdEvent(holdEvent),
    isPressed(false),
    pressTimestamp(0),
    releaseTimestamp(0)
{}

void Button::buttonISR(void* instancePtr) {
    Button* instance = reinterpret_cast<Button*>(instancePtr);
    
    pageEvent_t eventToSend;
    BaseType_t ign; // Would be truthy if a higher-priority task was unblocked by queueing an event, but there's no blocking queue code
    if (!digitalRead(instance->pin)) {    // Pin is pulled up, so pressing the button creates a falling edge
    instance->pressTimestamp = millis();
    instance->isPressed = true;
    eventToSend = instance->pressEvent;
    if (millis() - instance->releaseTimestamp > DEBOUNCE_TIME)
        xQueueSendFromISR(instance->eventQueue, &eventToSend, &ign);
    }
    else {
    instance->releaseTimestamp = millis();
    instance->isPressed = false;
    if (millis() - instance->pressTimestamp > LONGPRESS_TIME)
        eventToSend = instance->holdEvent;
    else
        eventToSend = instance->bumpEvent;
    if (millis() - instance->pressTimestamp > DEBOUNCE_TIME)
        xQueueSendFromISR(instance->eventQueue, &eventToSend, &ign);
    }
}

void Button::attach() {
    pinMode(pin, INPUT_PULLUP);
    attachInterruptArg(pin, &Button::buttonISR, this, CHANGE);
}