#include <stack>
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
    activeBuffer->fillRect(0, 0, 240, SBAR_HEIGHT, ACCENT_COLOR);
    activeBuffer->fillRoundRect(210, 2, 18, 11, 2, BGND_COLOR);
    activeBuffer->fillRoundRect(226, 5, 5, 5, 2, BGND_COLOR);
    activeBuffer->fillRoundRect(212, 4, 14 * (batPercentage / 100.0), 7, 2, TEXT_COLOR);
}

void Display::pushChanges() {
    activeBuffer->pushSprite(0, 0);
    activeBuffer = (activeBuffer == bufferA) ? bufferB : bufferA;
}



DisplayPage::DisplayPage(Display* display, DisplayManager* displayManager, const char* pageName) {
    this->display = display;
    this->displayManager = displayManager;
    this->pageName = pageName;
    this->frameCounter = 0;
}

DisplayPage::~DisplayPage() {}



MenuPage::~MenuPage() {
    delete[] this->memberPages;
}

// Draw the menu if no subpage is active; otherwise, draw the active subpage
void MenuPage::draw() {
    // Draw the menu, highlighting item # subpageIdx
    display->textFormat(2, TFT_WHITE);
    display->drawString(pageName, 30, 30);
    display->drawString("Selection: " + String(subpageIdx), 30, 60);
    display->drawString(String(millis()), 30, 90);
    this->frameCounter++;
}

// Callback runs when an event appears in the event queue
void MenuPage::onEvent(pageEvent_t event) {
    switch (event) {
        case pageEvent_t::ENTER:
            break;
        case pageEvent_t::EXIT:
            break;
        case pageEvent_t::NAV_UP:
            if (subpageIdx > 0)
                subpageIdx--;
            break;
        case pageEvent_t::NAV_DOWN:
            if (subpageIdx < numPages - 1)
                subpageIdx++;
            break;
        case pageEvent_t::NAV_SELECT: {
            memberPages[subpageIdx]->onEvent(pageEvent_t::ENTER);
            displayManager->pageStack.push(memberPages[subpageIdx]);
        }   break;
        case pageEvent_t::NAV_CANCEL:
            displayManager->pageStack.pop();
            break;
    }
}



HomePage::HomePage(Display* display, DisplayManager* displayManager, const char* pageName, MenuPage* mainMenu) : DisplayPage(display, displayManager, pageName) {
    this->mainMenu = mainMenu;
}

void HomePage::draw() {
    display->textFormat(2, TFT_WHITE);
    display->drawString("Battery Life:", 30,30);
    display->textFormat(3, TFT_WHITE);
    display->drawString(String(getBatteryPercentage()) + "%", 30, 50);
    frameCounter++;
}

void HomePage::onEvent(pageEvent_t event) {
    switch (event) {
        case pageEvent_t::NAV_SELECT:
            displayManager->pageStack.push(mainMenu);
            break;
        default:
            break;
    }
}



DisplayManager::DisplayManager(Display* display) {
    this->display = display;
    this->eventQueue = xQueueCreate(4, sizeof(pageEvent_t));
}

void DisplayManager::setHomepage(HomePage* homepage) {
    assert(this->pageStack.empty());
    pageStack.push(homepage);
}

void DisplayManager::draw() {
    // Forward any events to the active page
    if (uxQueueMessagesWaiting(eventQueue)) {
        pageEvent_t event;
        xQueueReceive(eventQueue, &event, 0);
        // Serial.printf("Event received: %i\n", event);
        this->pageStack.top()->onEvent(event);
    }

    // Draw the status bar and the active page
    display->drawStatusBar();
    pageStack.top()->draw();
}