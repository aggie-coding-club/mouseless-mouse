#include <TFT_eSPI.h>
#include "display.h"

#define PWM_CHANNEL 0
#define BACKLIGHT_PIN 4

Display::Display(TFT_eSPI* tft, TFT_eSprite* bufferA, TFT_eSprite* bufferB) {
    this->tft = tft;
    this->bufferA = bufferA;
    this->bufferB = bufferB;
    this->activeBuffer = bufferA;
    this->fillColor = TFT_WHITE;
    this->strokeColor = TFT_WHITE;
    bufferA->createSprite(240, 135);
    bufferB->createSprite(240, 135);
}

Display::~Display() {
    delete bufferA;
    delete bufferB;
}

void Display::begin() {
    ledcAttachPin(BACKLIGHT_PIN, PWM_CHANNEL);
    ledcSetup(PWM_CHANNEL, 20000, 8);
    ledcWrite(PWM_CHANNEL, 255);
    tft->init();
    tft->initDMA();
    tft->setRotation(1);
    tft->fillScreen(TFT_BLACK);
}

void Display::dim(uint8_t brightness) {
    ledcWrite(PWM_CHANNEL, brightness);
}

void Display::clear() {
    activeBuffer->fillSprite(TFT_BLACK);
}

void Display::setFill(uint16_t color) {
    this->fillColor = color;
}

void Display::setStroke(uint16_t color) {
    this->strokeColor = color;
}

void Display::drawString(String string, uint16_t xPos, uint16_t yPos) {
    activeBuffer->drawString(string, xPos, yPos);
}

void Display::drawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
    activeBuffer->drawLine(x1, y1, x2, y2, strokeColor);
}

void Display::textFormat(uint8_t size, uint16_t color) {
    activeBuffer->setTextSize(size);
    activeBuffer->setTextColor(color);
}

void Display::pushChanges() {
    activeBuffer->pushSprite(0, 0);
    activeBuffer = (activeBuffer == bufferA) ? bufferB : bufferA;
}

// DisplayPage class implementation, which is intended to hold a graphical menu page
DisplayPage::DisplayPage(void (*drawHandler)()) {
    this->drawHandler = drawHandler;
}

inline void DisplayPage::draw() {
    this->drawHandler();
}

// MenuPage class implementation, which is intended to hold submenus
template <typename... T>
MenuPage::MenuPage(const T*... menuPages) {
    this->numPages = sizeof...(menuPages);
    this->pages = malloc(numPages * sizeof(DisplayPage));
    const DisplayPage* pagesToInitialize[numPages] = {menuPages...};
    for (byte i = 0; i < numPages; i++) {
        this->pages[i] = pagesToInitialize[i];
    }
}