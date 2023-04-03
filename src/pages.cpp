#include "display.h"
#include "helpers.h"
#include "pages.h"
#include "mouse.h"
#include "imgs/hand.h"


extern xQueueHandle mouseQueue;
// Not much to do for a blank page
BlankPage::BlankPage(Display* display, DisplayManager* displayManager, const char* pageName) : DisplayPage(display, displayManager, pageName) {}

// Great place for debug stuff
void BlankPage::draw() {
    display->textFormat(2, TFT_WHITE);
    display->drawString(pageName, 30, 30);
    display->drawString(String(touchRead(T7)), 30, 60);
    display->drawNavArrow(120, 110, pageName[12]&1, 0.5 - 0.5*cos(6.28318*float(frameCounter%90)/90.0), 0x461F, TFT_BLACK);
    frameCounter++;
};

// This page doesn't do a lot, so it only needs to handle exiting
void BlankPage::onEvent(pageEvent_t event) {
    if (event == pageEvent_t::NAV_CANCEL) this->displayManager->pageStack.pop();
};



// Create a keyboard page attached to a display and manager
KeyboardPage::KeyboardPage(Display* display, DisplayManager* displayManager, const char* pageName) :
    DisplayPage(display, displayManager, pageName), textBuffer(new char[32]),
    bufferIdx(0), colIdx(1), letterIdx(0), numberIdx(0), letterTlY(0), numberTlY(0)
{
    for (byte i = 0; i < 32; i++) textBuffer[i] = '\0';
}

// Attach the keyboard page instance to a text field
KeyboardPage* KeyboardPage::operator() (char* field) {
    this->field = field;
    return this;
}

// Draw the keyboard page
void KeyboardPage::draw() {
    display->textFormat(2, TFT_WHITE);
    display->setFill(SEL_COLOR);
    display->setStroke(TFT_WHITE);
    display->drawString(textBuffer, 20, 30);
    display->fillRect(130 + 30*colIdx, 78, 30, 30);
    for (int16_t i = -1; i < 2; i++) {
        if ((i + specialIdx) % 3_pm == 0) {
            display->drawLine(140, 85 + 30*i + specialTlY, 154, 85 + 30*i + specialTlY);
            display->drawLine(154, 85 + 30*i + specialTlY, 154, 99 + 30*i + specialTlY);
            display->drawLine(154, 99 + 30*i + specialTlY, 140, 99 + 30*i + specialTlY);
            display->drawLine(140, 99 + 30*i + specialTlY, 134, 92 + 30*i + specialTlY);
            display->drawLine(134, 92 + 30*i + specialTlY, 140, 85 + 30*i + specialTlY);
        }
        else if ((i + specialIdx) % 3_pm == 1) {
            display->drawLine(140, 99 + 30*i + specialTlY, 148, 99 + 30*i + specialTlY);
            display->drawLine(140, 96 + 30*i + specialTlY, 140, 99 + 30*i + specialTlY);
            display->drawLine(148, 96 + 30*i + specialTlY, 148, 99 + 30*i + specialTlY);
        }
        else if ((i + specialIdx) % 3_pm == 2) {
            display->drawNavArrow(145, 93 + 30*i + specialTlY, false, 0.5 - 0.5*cos(6.28318*float(frameCounter%90)/90.0), ACCENT_COLOR, BGND_COLOR);
        }
    }
    for (int16_t i = -1; i < 2; i++) {
        display->drawString(String(char('A' + (i + letterIdx) % 26_pm)), 170, 85 + 30*i + letterTlY);
    }
    for (int16_t i = -1; i < 2; i++) {
        display->drawString(String(char('0' + (i + numberIdx) % 10_pm)), 200, 85 + 30*i + numberTlY);
    }
    specialTlY *= 0.5;
    letterTlY *= 0.5;
    numberTlY *= 0.5;
    frameCounter++;
}

// Event handling for the keyboard
void KeyboardPage::onEvent(pageEvent_t event) {
    switch (event) {
        case pageEvent_t::NAV_UP: {
            if (colIdx == 0) {
                specialIdx--;
                specialTlY -= 30;
            }
            else if (colIdx == 1) {
                letterIdx--;
                letterTlY -= 30;
            }
            else if (colIdx == 2) {
                numberIdx--;
                numberTlY -= 30;
            }
        }   break;
        case pageEvent_t::NAV_DOWN: {
            if (colIdx == 0) {
                specialIdx++;
                specialTlY += 30;
            }
            else if (colIdx == 1) {
                letterIdx++;
                letterTlY += 30;
            }
            else if (colIdx == 2) {
                numberIdx++;
                numberTlY += 30;
            }
        }   break;
        case pageEvent_t::NAV_SELECT: {
            if (colIdx == 0 && specialIdx % 3_pm == 2) {
                memcpy(field, textBuffer, bufferIdx);
                for (byte i = 0; i < 32; i++) textBuffer[i] = '\0';
                specialIdx = 0;
                letterIdx = 0;
                numberIdx = 0;
                colIdx = 1;
                displayManager->pageStack.pop();
            }
            else if (bufferIdx < 31) {
                if (colIdx == 0 && specialIdx % 3_pm == 0 && bufferIdx > 0) textBuffer[--bufferIdx] = '\0';
                else if (colIdx == 0 && specialIdx % 3_pm == 1) textBuffer[bufferIdx++] = ' ';
                else if (colIdx == 1) textBuffer[bufferIdx++] = char('A' + letterIdx % 26_pm);
                else if (colIdx == 2) textBuffer[bufferIdx++] = char('0' + numberIdx % 10_pm);
            }
        }   break;
        case pageEvent_t::NAV_CANCEL:
            --colIdx %= 3_pm;
            break;
        default:
            break;
    }
}



// Create a confirmation page attached to a display and manager
ConfirmationPage::ConfirmationPage(Display* display, DisplayManager* displayManager, const char* pageName) :
    DisplayPage(display, displayManager, pageName)
{}

// Attach the confirmation page to a specific prompt message and action
ConfirmationPage* ConfirmationPage::operator() (const char* message, void (*callback)()) {
    this->message = message;
    this->callback = callback;
    return this;
}

// Draw the confirmation page
void ConfirmationPage::draw() {
    display->textFormat(2, TFT_WHITE);
    display->drawString(message, 10, 30);
}

// Event handling for the confirmation page
void ConfirmationPage::onEvent(pageEvent_t event) {
    switch (event) {
        case pageEvent_t::NAV_CANCEL:
            displayManager->pageStack.pop();
            break;
        case pageEvent_t::NAV_SELECT:
            callback();
            break;
        default:
            break;
    }
}



InputDisplay::InputDisplay(Display* display, DisplayManager* displayManager, const char* pageName) : DisplayPage(display, displayManager, pageName) {}
void InputDisplay::draw() {
    // if (uxQueueMessagesWaiting(mouseQueue)) {
    //     mouseEvent_t event;
    //     xQueueReceive(mouseQueue, &event, 0);
    //     switch(event) {
    //         case mouseEvent_t::LMB_PRESS:
    //             lmb = true;
    //             break;
    //         case mouseEvent_t::LMB_RELEASE:
    //             lmb = false;
    //             break;
    //         case mouseEvent_t::RMB_PRESS:
    //             rmb = true;
    //             break;
    //         case mouseEvent_t::RMB_RELEASE:
    //             lmb = false;
    //             break;
    //         case mouseEvent_t::SCROLL_UP:
    //             scrollU = true;
    //             break;
    //         case mouseEvent_t::SCROLL_DOWN:
    //             scrollD = true;
    //             break;
    //         default:
    //             break;
    //     }
    //     if(lmb) {
    //         display->drawBitmapSPIFFS("/pointer.bmp",60,60);
    //     }
    //     if(rmb) {
    //         display->drawBitmapSPIFFS("/middle.bmp",60,60);
    //     }
    //     if(scrollU) {
    //         display->drawBitmapSPIFFS("/scrollUp.bmp",60,60);
    //     }
    //     if(scrollD) {
    //         display->drawBitmapSPIFFS("/scrollDown.bmp",60,60);
    //     }
    // }
    display->textFormat(2, TFT_WHITE);
    display->drawString(pageName, 30, 30);
    display->pushImage(60,60,64,64, hand);
}
void InputDisplay::onEvent(pageEvent_t event) {
    if (event == pageEvent_t::NAV_CANCEL) this->displayManager->pageStack.pop();
}