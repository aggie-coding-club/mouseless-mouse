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
    releaseTimestamp(0),
    stateChangeTimestamp(0)
{
    debounceTimer = xTimerCreate(
        "Button Debounce Callback",     // Timer name
        pdMS_TO_TICKS(DEBOUNCE_TIME),   // Timer duration
        pdFALSE,                        // Don't auto-restart timer on expiry
        this,
        &Button::finalDebounce
    );
}

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
    detachInterrupt(instance->pin);
    portYIELD_FROM_ISR(wokeTask);
}

void Button::finalDebounce(xTimerHandle timer) {
    Button* instance = reinterpret_cast<Button*>(pvTimerGetTimerID(timer));
    bool pinState = !digitalRead(instance->pin);
    pageEvent_t eventToSend;
    if (pinState == instance->isPressed) {
        attachInterruptArg(instance->pin, &Button::buttonISR, instance, CHANGE);
        return;  // The final state set by the actual ISR was correct
    }
    // If the pin state has not changed, execution will not reach here
    if (pinState == true) {
        eventToSend = instance->pressEvent;
        instance->pressTimestamp = millis();
        instance->isPressed = true;
        xQueueSend(instance->eventQueue, &eventToSend, 0);
    }
    else {
        eventToSend = instance->bumpEvent;
        instance->releaseTimestamp = millis();
        instance->isPressed = false;
        xQueueSend(instance->eventQueue, &eventToSend, 0);
    }
    xTimerReset(timer, 0);
}

void Button::attach() {
    pinMode(pin, INPUT_PULLUP);
    attachInterruptArg(pin, &Button::buttonISR, this, CHANGE);
}