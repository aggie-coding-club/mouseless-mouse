#include <stack>
#include <TFT_eSPI.h>
#include "display.h"
#include "io.h"

inline uint16_t wrapDegrees(int16_t degrees) {
    return degrees % 360 + 360 * (degrees < 0);
}

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
    ledcWrite(PWM_CHANNEL, BRIGHT_BRIGHTNESS);
    tft->init();
    tft->initDMA();
    tft->setRotation(1);
    tft->fillScreen(TFT_BLACK);
}

void Display::dim(uint8_t brightness) {
    this->brightness = brightness;
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

int16_t Display::getStringWidth(const char* string) {
    return activeBuffer->textWidth(string);
}

void Display::drawString(String string, uint16_t xPos, uint16_t yPos) {
    activeBuffer->drawString(string, xPos, yPos);
}

void Display::drawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
    activeBuffer->drawLine(x1, y1, x2, y2, strokeColor);
}

void Display::drawArc(uint16_t x, uint16_t y, uint16_t r, uint16_t ir, uint16_t startAngle, uint16_t endAngle, uint16_t fg_color, uint16_t bg_color) {
    activeBuffer->drawArc(x, y, r, ir, startAngle, endAngle, fg_color, bg_color, false);
}

void Display::fillRect(uint16_t x1, uint16_t y1, uint16_t width, uint16_t height) {
    activeBuffer->fillRect(x1, y1, width, height, fillColor);
}

void Display::textFormat(uint8_t size, uint16_t color) {
    bufferA->setTextSize(size);
    bufferB->setTextSize(size);
    bufferA->setTextColor(color);
    bufferB->setTextColor(color);
}

void Display::pushChanges() {
    activeBuffer->pushSprite(0, 0);
    activeBuffer = (activeBuffer == bufferA) ? bufferB : bufferA;
}

void Display::drawStatusBar() {
    byte batPercentage = getBatteryPercentage();
    activeBuffer->fillRect(0, 0, 240, SBAR_HEIGHT, ACCENT_COLOR);
    activeBuffer->fillRoundRect(210, 2, 18, 11, 2, BGND_COLOR);
    activeBuffer->fillRoundRect(226, 5, 5, 5, 2, BGND_COLOR);
    activeBuffer->fillRoundRect(212, 4, 14 * (batPercentage / 100.0), 7, 2, TEXT_COLOR);
}

void Display::drawNavArrow(uint16_t x, uint16_t y, bool direction, float progress, uint16_t stroke_color, uint16_t bg_color) {
    progress = min(1.0F, progress);
    int16_t flip = direction ? -1 : 1;
    uint16_t arrowheadX;
    uint16_t arrowheadY;
    float arrowheadAngle;
    float arrowTailAngle;
    float arrowheadAngleRads;
    uint16_t arrowheadLength = 5;
    float arrowheadBreadth = 0.6;
    if (progress < 0.5)
        activeBuffer->drawLine(x, y + 10*flip, x, y - 5*flip, stroke_color);
    else if (progress < 0.75)
        activeBuffer->drawLine(x, y + (40 - 60 * progress)*flip, x, y - 5*flip, stroke_color);
    arrowheadX = 5 - 5 * cos(6.28318 * 3 * min(float(0.25), progress));
    arrowheadY = -5 - 5 * sin(6.28318 * 3 * min(float(0.25), progress));
    if (progress > 0.25) {
        activeBuffer->drawLine(x + 5*flip, y, x + max(float(-10), 20 - 60*progress)*flip, y, stroke_color);
        arrowheadX += 15 - 60*min(float(0.5), progress);
    }
    arrowheadX = arrowheadX * flip + x;
    arrowheadY = arrowheadY * flip + y;
    arrowheadAngle = 180*!direction - 90 + 360 * 3 * min(float(0.25), progress); // TFT_eSPI defines 0 degrees as straight down and positive as clockwise
    arrowTailAngle = 180*!direction - 90 + 360 * 3 * max(float(0), progress - float(0.75));
    arrowheadAngleRads = 3.14159*(0.5 + direction) - 6.28318 * 3 * min(float(0.25), progress); // C++ uses real math
    if (progress > 0 && progress < 1)
        activeBuffer->drawArc(x + 5*flip, y - 5*flip, 5, 5, wrapDegrees(arrowTailAngle), wrapDegrees(arrowheadAngle), stroke_color, bg_color);
    activeBuffer->fillTriangle(
        arrowheadX,
        arrowheadY,
        arrowheadX - arrowheadLength*cos(arrowheadAngleRads - arrowheadBreadth),
        arrowheadY + arrowheadLength*sin(arrowheadAngleRads - arrowheadBreadth),
        arrowheadX - arrowheadLength*cos(arrowheadAngleRads + arrowheadBreadth),
        arrowheadY + arrowheadLength*sin(arrowheadAngleRads + arrowheadBreadth),
        stroke_color
    );
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
    display->textFormat(1, TFT_BLACK);
    display->drawString(pageName, (240 - display->getStringWidth(pageName)) / 2, 4);
    display->textFormat(2, TFT_WHITE);
    for (byte i = 0; i < numPages; i++) {
        if (15 + i*30 + menuTlY < -15) continue;    // No reason to draw things that are out of frame
        if (15 + i*30 + menuTlY > 135) break;
        if (i == subpageIdx) {
            display->setFill(SEL_COLOR);
            display->fillRect(0, 15 + i*30 + selectionTlY + menuTlY, 240, 30);
            if (displayManager->upButton->isPressed || displayManager->downButton->isPressed) {
                Button* activeButton = displayManager->upButton->isPressed ? displayManager->upButton : displayManager->downButton;
                display->drawNavArrow(
                    220, 30 + i*30 + selectionTlY + menuTlY,
                    displayManager->upButton->isPressed,
                    pow(millis() - activeButton->pressTimestamp, 2) / pow(LONGPRESS_TIME, 2),
                    ACCENT_COLOR,
                    SEL_COLOR
                );
            }
        }
        display->drawString(memberPages[i]->pageName, 30, 25 + i*30 + menuTlY);
    }
    int16_t targetMenuTlY = min(0, 40 - 30*subpageIdx);
    menuTlY += 0.25 * (targetMenuTlY - menuTlY);
    selectionTlY *= 0.75;
    this->frameCounter++;
}

// Callback runs when an event appears in the event queue
void MenuPage::onEvent(pageEvent_t event) {
    switch (event) {
        case pageEvent_t::ENTER:
            break;
        case pageEvent_t::EXIT:
            break;
        case pageEvent_t::NAV_PRESS:
            break;
        case pageEvent_t::NAV_UP:
            if (subpageIdx > 0) {
                subpageIdx--;
                selectionTlY += 30;
            }
            break;
        case pageEvent_t::NAV_DOWN:
            if (subpageIdx < numPages - 1) {
                subpageIdx++;
                selectionTlY -= 30;
            }
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

void DisplayManager::attachButtons(Button* upButton, Button* downButton) {
    this->upButton = upButton;
    this->downButton = downButton;
}

void DisplayManager::draw() {
    // Forward any events to the active page
    if (uxQueueMessagesWaiting(eventQueue)) {
        pageEvent_t event;
        xQueueReceive(eventQueue, &event, 0);
        lastEventFrame = frameCtr;
        if (display->brightness < BRIGHT_BRIGHTNESS)
            display->dim(BRIGHT_BRIGHTNESS);
        this->pageStack.top()->onEvent(event);
    }
    else if (display->brightness > DIM_BRIGHTNESS && lastEventFrame + 900 < frameCtr)
        display->dim(max((int)DIM_BRIGHTNESS, display->brightness - 5));

    // Draw the status bar and the active page
    pageStack.top()->draw();
    display->drawStatusBar();
    frameCtr++;
}