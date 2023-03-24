#include <TFT_eSPI.h>
#include "display.h"

#define PWM_CHANNEL 0
#define BACKLIGHT_PIN 4

const uint8_t SBAR_HEIGHT = 15;         // Height of the status bar in pixels
const uint16_t ACCENT_COLOR = 0x461F;   // TFT_eSPI::color565(64, 192, 255)
const uint16_t TEXT_COLOR = TFT_WHITE;  // Color of menu text
const uint16_t BGND_COLOR = TFT_BLACK;  // Color of background

extern int16_t getBatteryPercentage();

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
    bufferA->setTextSize(size);
    bufferB->setTextSize(size);
    bufferA->setTextColor(color);
    bufferB->setTextColor(color);
}

void Display::drawStatusBar() {
    byte batPercentage = getBatteryPercentage();
    activeBuffer->drawRect(0, 0, 240, SBAR_HEIGHT, ACCENT_COLOR);
    activeBuffer->drawRoundRect(210, 2, 18, 11, 3, BGND_COLOR);
    activeBuffer->drawRoundRect(212, 4, 14 * (batPercentage / 100.0), 7, 3, TEXT_COLOR);
}

void Display::pushChanges() {
    activeBuffer->pushSprite(0, 0);
    activeBuffer = (activeBuffer == bufferA) ? bufferB : bufferA;
}



DisplayPage::DisplayPage(Display* display, xQueueHandle eventQueue, const char* pageName) {
    this->display = display;
    this->eventQueue = eventQueue;
    this->pageName = pageName;
}

DisplayPage::~DisplayPage() {}



template <typename... Ts>
MenuPage<Ts...>::MenuPage(Display* display, QueueHandle_t eventQueue, const char* pageName, Ts*... pages) : DisplayPage(display, eventQueue, pageName) {
    this->numPages = sizeof...(Ts);
    this->memberPages = new DisplayPage*[numPages];
    *(this->memberPages) = {pages...};
    this->subpageActive = false;
    this->subpageIdx = 0;
}

template <typename... Ts>
MenuPage<Ts...>::~MenuPage() {
    delete[] this->memberPages;
}

// Check if the menu itself is the active page
template <typename... Ts>
inline bool MenuPage<Ts...>::isActive() {
    return !this->subpageActive;
}

// Draw the menu if no subpage is active; otherwise, draw the active subpage
template <typename... Ts>
void MenuPage<Ts...>::draw() {
    if (subpageActive) {
        this->memberPages[subpageIdx]->draw();
    }
    else {
        // Draw the menu, highlighting item # subpageIdx
    }
}

// Callback runs when an event appears in the event queue
template <typename... Ts>
void MenuPage<Ts...>::onEvent(pageEvent_t event) {
    if (subpageActive) {
        this->memberPages[subpageIdx]->onEvent(event);
    }
    else switch (event) {
        case NAV_ENTER:
            break;
        case NAV_EXIT:
            break;
        case NAV_UP:
            if (subpageIdx > 0)
                subpageIdx--;
            break;
        case NAV_DOWN:
            if (subpageIdx < numPages - 1)
                subpageIdx++;
            break;
        case NAV_SELECT:
            subpageActive = true;
            break;
    }
}



template <typename... Ts>
HomePage<Ts...>::HomePage(Display* display, xQueueHandle eventQueue, const char* pageName, MenuPage<Ts...>* mainMenu) : DisplayPage(display, eventQueue, pageName) {
    this->mainMenu = mainMenu;
    this->menuActive = false;
}

template <typename... Ts>
void HomePage<Ts...>::draw() {
    if (menuActive) {
        mainMenu->draw();
    }
    else {
        this->display->drawStatusBar();
    }
}

template <typename... Ts>
void HomePage<Ts...>::onEvent(pageEvent_t event) {
    if (menuActive) {
        mainMenu->onEvent(event);
    }
    else switch (event) {
        case NAV_SELECT:
            menuActive = true;
            break;
        default:
            break;
    }
}